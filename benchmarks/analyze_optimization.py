"""Summarize paired timing rounds and independent pre-barrier traces."""

import argparse
from collections import defaultdict
import csv
from dataclasses import asdict
import json
from pathlib import Path
import statistics

from analyze import export_figure, summarize
import matplotlib.pyplot as plt


SOLVERS = ("sequential", "openmp", "dynamic16", "dynamic64")
LABELS = {"openmp": "Static", "dynamic16": "Dynamic 16", "dynamic64": "Dynamic 64"}
Row = dict[str, str]
Result = dict[str, str | int | float]


def load_campaign(directory: Path) -> tuple[dict, list[Row]]:
    metadata = json.loads((directory / "metadata.json").read_text(encoding="utf-8"))
    with (directory / "raw.csv").open(newline="", encoding="utf-8") as stream:
        return metadata, list(csv.DictReader(stream))


def write_csv(path: Path, rows: list[Result]) -> None:
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def check_settings(metadata: dict, row: Row, settings: dict[int, tuple[str, ...]]) -> None:
    n = int(row["n"])
    if n not in metadata["sizes"] or int(row["k"]) != metadata["steps"] or int(row["seed"]) != metadata["seed"]:
        raise ValueError("Raw input differs from its declared size, step count or seed.")
    physical_case = tuple(row[key] for key in ("k", "seed", "g", "epsilon", "dt", "initial_hash"))
    if settings.setdefault(n, physical_case) != physical_case:
        raise ValueError(f"Incompatible input states or model settings for N={n}.")


def check_timing_rounds(metadata: dict, rows: list[Row], settings: dict[int, tuple[str, ...]]) -> None:
    expected = {("sequential", 1)}
    expected.update((solver, p) for solver in SOLVERS[1:] for p in metadata["requested_openmp_teams"])
    rounds = range(1, metadata["measured_rounds"] + 1)
    by_round = defaultdict(list)
    for row in rows:
        check_settings(metadata, row, settings)
        by_round[int(row["n"]), int(row["round"])].append(row)
    if set(by_round) != {(n, r) for n in metadata["sizes"] for r in rounds}:
        raise ValueError("Missing or extra timing round.")
    for key, group in by_round.items():
        configurations = {(row["solver"], int(row["requested_threads"])) for row in group}
        orders = sorted(int(row["order"]) for row in group)
        if configurations != expected or orders != list(range(1, len(expected) + 1)):
            raise ValueError(f"Incomplete or repeated timing configuration in {key}.")


def timing_results(label: str, rows: list[Row]) -> list[Result]:
    summaries = summarize(rows, SOLVERS)
    static = {(s.n, s.threads): s.median_s for s in summaries if s.solver == "openmp"}
    samples = {(int(r["n"]), int(r["requested_threads"]), r["solver"], int(r["round"])):
               float(r["elapsed_s"]) for r in rows}
    result = []
    for s in summaries:
        ratio = ""
        wins = ""
        if s.solver != "sequential":
            ratio = static[s.n, s.threads] / s.median_s
        if s.solver in SOLVERS[2:]:
            wins = sum(
                samples[s.n, s.threads, s.solver, r] < samples[s.n, s.threads, "openmp", r]
                for r in range(1, s.runs + 1)
            )
        result.append({"pass": label, **asdict(s), "static_over_solver": ratio, "paired_wins": wins})
    return result


def arrival_results(
    metadata: dict, rows: list[Row], settings: dict[int, tuple[str, ...]]
) -> tuple[list[Result], list[Result]]:
    groups = defaultdict(dict)
    for row in rows:
        check_settings(metadata, row, settings)
        p = int(row["requested_threads"])
        key = (int(row["n"]), p, row["solver"], int(row["round"]), int(row["evaluation"]))
        thread = int(row["thread_id"])
        targets, finish = int(row["targets"]), int(row["finish_ns"])
        if (int(row["observed_threads"]) != p or thread in groups[key]
                or not 0 <= thread < p or min(targets, finish) < 0):
            raise ValueError(f"Invalid or duplicate worker in {key}.")
        groups[key][thread] = (targets, finish)
    expected = {
        (n, p, solver, r, e)
        for n in metadata["sizes"] for p in metadata["requested_openmp_teams"]
        for solver in SOLVERS[1:] for r in range(1, metadata["measured_rounds"] + 1)
        for e in range(metadata["steps"] + 1)
    }
    if set(groups) != expected:
        raise ValueError("Missing or extra force evaluation.")
    details = []
    by_config = defaultdict(list)
    for key, workers in sorted(groups.items()):
        n, p, solver, repetition, evaluation = key
        counts, finishes = zip(*workers.values())
        if len(workers) != p or sum(counts) != n:
            raise ValueError(f"Incomplete coverage in {key}.")
        if solver == "openmp" and n % p == 0 and min(counts) != n // p:
            raise ValueError(f"Unexpected static distribution in {key}.")
        if solver != "openmp":
            chunk = int(solver.removeprefix("dynamic"))
            remainders = sorted(count % chunk for count in counts if count % chunk)
            if remainders != ([n % chunk] if n % chunk else []):
                raise ValueError(f"Targets do not form the declared chunks in {key}.")
        spread = (max(finishes) - min(finishes)) / 1e6
        sample = {"n": n, "threads": p, "solver": solver, "round": repetition,
                  "evaluation": evaluation, "spread_ms": spread,
                  "min_targets": min(counts), "max_targets": max(counts)}
        details.append(sample)
        by_config[n, p, solver].append(sample)
    summary = []
    for (n, p, solver), group in by_config.items():
        values = [sample["spread_ms"] for sample in group]
        q1, median, q3 = statistics.quantiles(values, n=4, method="inclusive")
        summary.append({"n": n, "threads": p, "solver": solver, "evaluations": len(values),
                        "median_ms": median, "q1_ms": q1, "q3_ms": q3, "iqr_ms": q3 - q1,
                        "min_targets": min(sample["min_targets"] for sample in group),
                        "max_targets": max(sample["max_targets"] for sample in group)})
    return details, summary


def plot_comparison(output: Path, results: list[Result]) -> None:
    pooled = [row for row in results if row["pass"] == "pooled"]
    sizes = sorted({row["n"] for row in pooled})
    figure, axes = plt.subplots(1, len(sizes), figsize=(7.5, 3.45), sharey=True, squeeze=False)
    figure.subplots_adjust(left=0.105, right=0.98, bottom=0.26, top=0.78, wspace=0.12)
    for panel, n in zip(axes[0], sizes):
        panel.axhline(1, color="#515d68", linestyle="--", label="Static reference")
        for solver, color, marker in [("dynamic16", "#14627d", "o"), ("dynamic64", "#ae4b19", "s")]:
            series = [r for r in pooled if r["solver"] == solver and r["n"] == n]
            threads = [r["threads"] for r in series]
            panel.plot(threads, [r["static_over_solver"] for r in series], color=color,
                       marker=marker, linewidth=1.9, label=LABELS[solver])
            panel.set_xticks(threads)
        panel.set_title(f"N = {n:,}")
        panel.set_xlabel("OpenMP threads")
        panel.spines[["top", "right"]].set_visible(False)
        panel.grid(axis="y", alpha=0.20)
    axes[0, 0].set_ylabel("Static / solver time (×)")
    figure.suptitle("Does dynamic scheduling reduce total time?", fontsize=14, weight="semibold")
    handles, labels = axes[0, 0].get_legend_handles_labels()
    figure.legend(handles, labels, ncols=3, loc="lower center", frameon=False)
    export_figure(figure, output, "schedule-comparison")


def plot_matched_rounds(output: Path, passes: dict[str, list[Row]]) -> None:
    """Show individual timings for the same primary case as the report table."""
    first_pass = next(iter(passes.values()))
    bodies = max(int(row["n"]) for row in first_pass)
    threads = max(int(row["requested_threads"]) for row in first_pass if int(row["n"]) == bodies)
    figure, panels = plt.subplots(1, len(passes), figsize=(7.2, 2.75), sharey=True, squeeze=False)
    figure.subplots_adjust(left=0.095, right=0.99, bottom=0.30, top=0.76, wspace=0.12)
    styles = [
        ("openmp", -0.16, "^", "#454a50"),
        ("dynamic16", 0.0, "o", "#14627d"),
        ("dynamic64", 0.16, "s", "#ae4b19"),
    ]
    for panel, (name, rows) in zip(panels[0], passes.items()):
        case = [row for row in rows if int(row["n"]) == bodies
                and int(row["requested_threads"]) == threads and row["solver"] != "sequential"]
        rounds = sorted({int(row["round"]) for row in case})
        for solver, offset, marker, color in styles:
            samples = [row for row in case if row["solver"] == solver]
            panel.scatter([int(row["round"]) + offset for row in samples],
                          [1000 * float(row["elapsed_s"]) for row in samples],
                          s=22, marker=marker, color=color, label=LABELS[solver], zorder=3)
        panel.set(xticks=rounds, xlim=(rounds[0] - 0.5, rounds[-1] + 0.5),
                  xlabel="Measured round", title=name.replace("-", " ").title())
        panel.grid(axis="y", alpha=0.23)
        panel.spines[["top", "right"]].set_visible(False)
    panels[0, 0].set_ylim(bottom=0)
    panels[0, 0].set_ylabel("Elapsed time (ms)")
    figure.suptitle(f"Matched simulation times at {bodies:,} bodies and {threads} threads", fontsize=13)
    handles, labels = panels[0, 0].get_legend_handles_labels()
    figure.legend(handles, labels, loc="lower center", ncols=3, frameon=False)
    export_figure(figure, output, "matched-rounds")


def write_report_tables(output: Path, results: list[Result], arrivals: list[Result]) -> None:
    """Export the primary timing case and the independent trace summary in display units."""
    n = max(row["n"] for row in results)
    p = max(row["threads"] for row in results if row["n"] == n)
    timing_lines = [
        r"\begin{tabular}{@{}lrrrrrr@{}}\toprule",
        r"Solver & $p$ & Median (ms) & IQR (ms) & $S_p$ & $E_p$ (\%) & Static / solver \\\midrule",
    ]
    for row in results:
        if row["pass"] != "pooled" or row["n"] != n:
            continue
        serial = row["solver"] == "sequential"
        if not serial and row["threads"] != p:
            continue
        name = "Sequential" if serial else LABELS[row["solver"]]
        ratio = "--" if serial else f"{row['static_over_solver']:.3f}"
        timing_lines.append(
            f"{name} & {row['threads']} & {1000 * row['median_s']:.2f} & "
            f"{1000 * row['iqr_s']:.2f} & {row['speedup']:.2f} & "
            f"{100 * row['efficiency']:.1f} & {ratio} " + r"\\"
        )
    arrival_lines = [
        r"\begin{tabular}{@{}rlrrr@{}}\toprule",
        r"$p$ & Schedule & Median (ms) & $Q_1$--$Q_3$ (ms) & Targets per worker \\\midrule",
    ]
    for row in sorted(arrivals, key=lambda r: (r["n"], r["threads"], SOLVERS.index(r["solver"]))):
        if row["n"] == n:
            arrival_lines.append(
                f"{row['threads']} & {LABELS[row['solver']]} & {row['median_ms']:.2f} & "
                f"{row['q1_ms']:.2f}--{row['q3_ms']:.2f} & "
                f"{row['min_targets']}--{row['max_targets']} " + r"\\"
            )
    for name, lines in [("primary-timings", timing_lines), ("trace-table", arrival_lines)]:
        lines.append(r"\bottomrule\end{tabular}")
        (output / f"{name}.tex").write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("campaign", type=Path, help="Parent of pass-a, pass-b and arrivals")
    parser.add_argument("--output", type=Path, help="Defaults to the campaign directory")
    args = parser.parse_args()
    output = args.output or args.campaign
    settings = {}
    results = []
    pooled = []
    offset = 0
    protocols = []
    passes = {}
    for name in ("pass-a", "pass-b"):
        metadata, rows = load_campaign(args.campaign / name)
        if metadata["mode"] != "compare":
            raise ValueError(f"Expected a timing comparison in {name}.")
        protocols.append((metadata["binary_sha256"], sorted(metadata["sizes"]),
                          sorted(metadata["requested_openmp_teams"]), metadata["measured_rounds"]))
        check_timing_rounds(metadata, rows, settings)
        passes[name] = rows
        results.extend(timing_results(name, rows))
        pooled.extend({**row, "round": str(int(row["round"]) + offset)} for row in rows)
        offset += metadata["measured_rounds"]
    if protocols[0] != protocols[1]:
        raise ValueError("Timing passes differ in binary, sizes, thread counts or repetitions.")
    results.extend(timing_results("pooled", pooled))
    metadata, traces = load_campaign(args.campaign / "arrivals")
    if (metadata["mode"] != "schedule-arrivals" or metadata["binary_sha256"] != protocols[0][0]
            or not set(metadata["sizes"]).issubset(settings)):
        raise ValueError("Expected schedule traces of a timed case from the same binary.")
    details, arrivals = arrival_results(metadata, traces, settings)
    output.mkdir(parents=True, exist_ok=True)
    write_csv(output / "summary.csv", results)
    write_csv(output / "arrival-spreads.csv", details)
    write_csv(output / "arrival-summary.csv", arrivals)
    write_report_tables(output, results, arrivals)
    plot_comparison(output, results)
    plot_matched_rounds(output, passes)


if __name__ == "__main__":
    main()
