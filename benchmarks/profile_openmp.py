"""Capture the baseline on Windows and resolve the executable's sampled addresses."""

import argparse
import csv
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import time

import psutil


ROOT = Path(__file__).resolve().parents[1]


def sha256(path: Path) -> str:
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def source_manifest() -> dict[str, str]:
    paths = [
        ROOT / "CMakeLists.txt",
        ROOT / "CMakePresets.json",
        ROOT / "benchmarks/benchmark.cpp",
        Path(__file__),
        ROOT / "src/gravity.cpp",
        ROOT / "src/gravity_kernel.hpp",
        ROOT / "src/simulation.cpp",
        *sorted((ROOT / "include/nbody").glob("*.hpp")),
    ]
    return {path.relative_to(ROOT).as_posix(): sha256(path) for path in paths}


def cpu_by_thread(process: psutil.Process) -> dict[int, float]:
    return {
        thread.id: thread.user_time + thread.system_time
        for thread in process.threads()
    }


def module_counts(events: str) -> dict[str, int]:
    pattern = r"(\d+) samples? across \d+ buckets? written to gmon\.out\.\d+\.([^\r\n]+)"
    return {name: int(hits) for hits, name in re.findall(pattern, events)}


def observe_threads(pid: int) -> dict:
    """Read two Windows CPU-accounting snapshots from the profiled child."""
    child = psutil.Process(pid)
    time.sleep(1)
    begin = time.perf_counter()
    first = cpu_by_thread(child)
    time.sleep(3)
    last = cpu_by_thread(child)
    return {
        "pid": pid,
        "wall_seconds": time.perf_counter() - begin,
        "cpu_delta_seconds_by_windows_tid": {
            tid: last[tid] - first[tid] for tid in sorted(first.keys() & last.keys())
        },
    }


def export_symbols(
    histogram: Path, debug_image: Path, output: Path, msys: Path, source_root: str
) -> int:
    """Map runtime histogram bins to the linked .text address before reading DWARF."""
    dump = subprocess.check_output(
        [msys / "usr/bin/gmondump.exe", "-v", histogram], text=True
    )
    span = re.search(r"address range (0x[0-9a-f]+)\.\.(0x[0-9a-f]+)", dump)
    sections = subprocess.check_output(
        [msys / "ucrt64/bin/objdump.exe", "-h", debug_image], text=True
    )
    section = re.search(r"^\s*\d+\s+\.text\s+(\w+)\s+(\w+)", sections, re.MULTILINE)
    if not span or not section:
        raise ValueError("Cannot identify both runtime and linked text ranges.")
    runtime_base, runtime_end = (int(value, 16) for value in span.groups())
    size, linked_base = (int(value, 16) for value in section.groups())
    if runtime_end - runtime_base != size:
        raise ValueError("Histogram extent differs from the executable's .text section.")
    buckets = re.findall(r"address (0x[0-9a-f]+), hitcount (\d+)", dump)
    addresses = [hex(linked_base + int(pc, 16) - runtime_base) for pc, _ in buckets]
    frames = subprocess.check_output(
        [msys / "ucrt64/bin/addr2line.exe", "-afiC", "-e", debug_image, *addresses],
        text=True,
    )
    by_address = {}
    for line in frames.splitlines():
        if re.fullmatch(r"0x[0-9a-fA-F]+", line):
            address = int(line, 16)
            by_address[address] = []
        else:
            by_address[address].append(line.removeprefix(source_root + "/"))
    with (output / "symbols.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow([
            "runtime_bin_start", "linked_bin_start", "samples", "inline_chain_leaf_to_outer"
        ])
        for (pc, hits), linked in zip(buckets, addresses, strict=True):
            lines = by_address[int(linked, 16)]
            pairs = zip(lines[::2], lines[1::2], strict=True)
            chain = [f"{name} @ {site}" for name, site in pairs]
            writer.writerow([pc, linked, hits, " <- ".join(chain)])
    return sum(int(hits) for _, hits in buckets)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True, help="Empty results directory")
    parser.add_argument("--msys", type=Path, default=Path("C:/msys64"))
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/ucrt64-profile")
    parser.add_argument("--replay", type=Path, help="Resolve a saved capture without a build")
    args = parser.parse_args()
    build = args.build_dir.resolve()
    executable = build / "nbody_benchmark.exe"
    profiler = args.msys / "usr/bin/profiler.exe"
    # Repeat the validated large case so the capture spans many sampling ticks.
    workload = [str(executable), "profile", "3072", "12", "20261117", "60", "4"]
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("Choose an empty output directory to preserve earlier captures.")
    if args.replay:
        saved = args.replay
        metadata = json.loads((saved / "capture.json").read_text(encoding="utf-8"))
        for name, expected in metadata["symbol_input_sha256"].items():
            if sha256(saved / name) != expected:
                raise ValueError(f"Archived symbol input checksum mismatch for {name}.")
        hits = export_symbols(
            saved / "executable.gmon", saved / "executable.debug", output, args.msys,
            metadata["source_root_at_build"],
        )
        print(f"Resolved {hits} executable samples from the saved capture.")
        return

    source_root = ROOT.as_posix()
    sources = source_manifest()
    head = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True)
    status = subprocess.check_output(
        ["git", "status", "--porcelain", "-uall"], cwd=ROOT, text=True
    )
    binary_hash = sha256(executable)
    scratch = Path(tempfile.mkdtemp(prefix="profile-", dir=build.parent))
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join(
        [str(args.msys / "ucrt64/bin"), str(args.msys / "usr/bin"), environment["PATH"]]
    )
    events_path = scratch / "events.txt"
    batches_path = scratch / "batches.txt"
    with batches_path.open("wb") as batches, events_path.open("wb") as events:
        with subprocess.Popen(
            workload, env=environment, stdout=batches, stderr=subprocess.STDOUT
        ) as child:
            # Attach after transient DLL mappings at program startup have disappeared.
            time.sleep(0.5)
            command = [str(profiler), "-v", "-e", "-s", "100", "-p", f"-{child.pid}"]
            with subprocess.Popen(command, cwd=scratch, env=environment, stdout=events,
                                  stderr=subprocess.STDOUT) as capture:
                window = observe_threads(child.pid)
                if capture.wait() != 0:
                    raise RuntimeError(f"Profile failed. Inspect {events_path}.")
            if child.wait() != 0:
                raise RuntimeError(f"Benchmark failed. Inspect {batches_path}.")
    events = events_path.read_text(encoding="utf-8", errors="replace")
    batch_log = batches_path.read_text(encoding="utf-8")
    completions = re.findall(r"profile_batch=(\d+) final_hash=([0-9a-f]+)", batch_log)
    if [int(batch) for batch, _ in completions] != list(range(1, 61)):
        raise ValueError("The profile did not complete 60 benchmark batches.")
    module_hits = module_counts(events)
    native = scratch / f"gmon.out.{window['pid']}.{executable.name}"
    histogram = output / "executable.gmon"
    shutil.copyfile(native, histogram)
    if sha256(executable) != binary_hash or source_manifest() != sources:
        raise ValueError("The binary or its sources changed during profiling.")
    debug_image = output / "executable.debug"
    subprocess.run(
        [args.msys / "ucrt64/bin/objcopy.exe", "--only-keep-debug", executable, debug_image],
        check=True,
    )
    hits = export_symbols(histogram, debug_image, output, args.msys, source_root)
    if hits != module_hits[executable.name]:
        raise ValueError("Executable sample count differs between the histogram and event log.")
    shutil.copyfile(events_path, output / "events.txt")
    shutil.copyfile(batches_path, output / "batches.txt")
    commands = json.loads((build / "compile_commands.json").read_text(encoding="utf-8"))
    metadata = {
        "head": head.strip(),
        "git_status_before": status.splitlines(),
        "source_root_at_build": source_root,
        "source_sha256": sources,
        "workload": workload,
        "attach_command": command,
        "profiler": subprocess.check_output([profiler, "--version"], text=True).splitlines()[0],
        "build_commands": [row["command"] for row in commands if Path(row["file"]).name in
                           {"gravity.cpp", "simulation.cpp", "benchmark.cpp"}],
        "binary_sha256": binary_hash,
        "symbol_input_sha256": {
            path.name: sha256(path) for path in (histogram, debug_image)
        },
        "samples_by_module": module_hits,
        "thread_cpu_window": window,
        "completed_batches": len(completions),
        "final_hashes": sorted({value for _, value in completions}),
    }
    (output / "capture.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"{len(completions)} batches, {hits} executable samples. See {output}.")


if __name__ == "__main__":
    main()
