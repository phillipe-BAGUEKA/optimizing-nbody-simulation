"""Run one unprofiled Release campaign per size, preserving raw rows and provenance."""

import argparse
import csv
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import re
import shlex
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def command_text(command: list[str]) -> str:
    return subprocess.check_output(command, cwd=ROOT, text=True).rstrip()


def git_provenance() -> tuple[str | None, list[str]]:
    """An extracted submission carries source hashes but has no checkout of its own."""
    if not (ROOT / ".git").exists():
        return None, []
    return (
        command_text(["git", "rev-parse", "HEAD"]),
        command_text(["git", "status", "--porcelain", "-uall"]).splitlines(),
    )


def read_cache(path: Path) -> dict[str, str]:
    cache = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line and not line.startswith(("#", "//")):
            key, value = line.split("=", 1)
            cache[key.split(":", 1)[0]] = value
    return cache


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sizes", type=int, nargs="+", required=True)
    parser.add_argument("--threads", type=int, nargs="+", default=[1, 2, 4, 8, 12])
    parser.add_argument("--steps", type=int, default=12)
    parser.add_argument("--seed", type=int, default=20261117)
    parser.add_argument("--repetitions", type=int, default=6)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/ucrt64-release")
    parser.add_argument("--output", type=Path, required=True, help="New output directory")
    options = parser.add_mutually_exclusive_group()
    parser.add_argument("--no-states", action="store_true", help="Omit final-state CSVs")
    options.add_argument("--arrivals", action="store_true", help="Record pre-barrier thread finishes")
    options.add_argument("--compare", action="store_true", help="Time static, dynamic16 and dynamic64")
    options.add_argument("--schedule-arrivals", action="store_true", help="Trace all three schedules")
    args = parser.parse_args()
    if len(set(args.sizes)) != len(args.sizes) or any(n < 1 for n in args.sizes):
        parser.error("Body counts must be distinct and positive.")

    build = args.build_dir.resolve()
    executable = build / "nbody_benchmark.exe"
    cache = read_cache(build / "CMakeCache.txt")
    if cache["CMAKE_BUILD_TYPE"] != "Release":
        parser.error("Use a Release build for this campaign.")
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    benchmark_command = next(
        row["command"] for row in commands if Path(row["file"]).name == "benchmark.cpp"
    )
    # Presets can cache the bare name g++; Ninja records the actual executable.
    compiler = Path(shlex.split(benchmark_command, posix=False)[0].strip('"'))
    if not compiler.is_absolute() or not compiler.is_file():
        parser.error("Cannot identify the actual compiler in compile_commands.json.")
    environment = os.environ.copy()
    environment["PATH"] = str(compiler.parent) + os.pathsep + environment["PATH"]
    sources = [
        ROOT / "CMakeLists.txt",
        ROOT / "CMakePresets.json",
        ROOT / "src/gravity.cpp",
        ROOT / "src/gravity_kernel.hpp",
        ROOT / "src/simulation.cpp",
        ROOT / "benchmarks/benchmark.cpp",
        ROOT / "benchmarks/run_campaign.py",
        *sorted((ROOT / "include/nbody").glob("*.hpp")),
    ]
    power_scheme = re.search(r"[\da-fA-F]{8}-(?:[\da-fA-F]{4}-){3}[\da-fA-F]{12}",
                             command_text(["powercfg", "/getactivescheme"]))
    mode = "campaign"
    if args.compare:
        mode = "compare"
    elif args.schedule_arrivals:
        mode = "schedule-arrivals"
    elif args.arrivals:
        mode = "arrivals"
    schedules = 3 if args.compare or args.schedule_arrivals else 1
    head, status = git_provenance()
    metadata = {
        "mode": mode,
        "head": head,
        "git_status_before": status,
        "source_sha256": {p.relative_to(ROOT).as_posix(): sha256(p) for p in sources},
        "binary_sha256": sha256(executable),
        "build_type": cache["CMAKE_BUILD_TYPE"],
        "compiler": command_text([str(compiler), "--version"]).splitlines()[0],
        "compile_commands": [
            entry["command"] for entry in commands
            if Path(entry["file"]).name in {"benchmark.cpp", "gravity.cpp", "simulation.cpp"}
        ],
        "windows_version": platform.version(),
        "logical_processors_reported_by_os": os.cpu_count(),
        "power_scheme_guid": power_scheme.group() if power_scheme else None,
        "openmp_environment": {
            key: value for key, value in environment.items() if key.startswith(("OMP_", "GOMP_"))
        },
        "steps": args.steps,
        "seed": args.seed,
        "sizes": args.sizes,
        "requested_openmp_teams": args.threads,
        "measured_rounds": args.repetitions,
    }
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("Choose an empty output directory to preserve previous measurements.")
    (output / "metadata.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    with (output / "raw.csv").open("w", newline="", encoding="utf-8") as raw:
        writer = csv.writer(raw, lineterminator="\n")
        for index, count in enumerate(args.sizes):
            command = [
                str(executable), mode, str(count), str(args.steps), str(args.seed),
                str(args.repetitions),
            ]
            if args.arrivals or args.schedule_arrivals:
                rows_per_round = schedules * (args.steps + 1) * sum(args.threads)
            else:
                state_path = "-" if args.no_states else str(output / f"final-{count}.csv")
                command.append(state_path)
                rows_per_round = 1 + schedules * len(args.threads)
            command.extend(map(str, args.threads))
            result = subprocess.run(
                command, env=environment, stdout=subprocess.PIPE, text=True, check=True
            )
            reader = csv.reader(io.StringIO(result.stdout))
            header = next(reader)
            if index == 0:
                writer.writerow(header)
            rows = list(reader)
            if len(rows) != args.repetitions * rows_per_round:
                raise ValueError(f"Incomplete campaign for N={count}.")
            writer.writerows(rows)
            raw.flush()
            print(f"N={count}: {len(rows)} validated measurements", file=sys.stderr, flush=True)


if __name__ == "__main__":
    main()
