"""Publish the speedup and optional thread-arrival figures from their raw CSVs."""

import argparse
import csv
from dataclasses import asdict, dataclass
import hashlib
import json
import math
from pathlib import Path
import platform
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.figure import Figure

matplotlib.rcParams.update({
    "font.family": "STIXGeneral",
    "mathtext.fontset": "cm",
    "pdf.fonttype": 42,
})


@dataclass
class Summary:
    n: int
    solver: str
    threads: int
    runs: int
    median_s: float
    q1_s: float
    q3_s: float
    iqr_s: float
    speedup: float
    efficiency: float


def summarize(
    rows: list[dict[str, str]], solvers: tuple[str, ...] = ("sequential", "openmp")
) -> list[Summary]:
    cases = sorted({int(row["n"]) for row in rows})
    if not cases:
        raise ValueError("The input contains no measurements.")
    summaries = []
    for n in cases:
        case = [row for row in rows if int(row["n"]) == n]
        settings = ("k", "seed", "g", "epsilon", "dt", "initial_hash")
        if len({tuple(row[key] for key in settings) for row in case}) != 1:
            raise ValueError(f"Cannot compare different initial conditions or settings for N={n}.")
        times = {}
        rounds = set(range(1, max(int(row["round"]) for row in case) + 1))
        for row in case:
            p = int(row["observed_control_threads"])
            key = (row["solver"], p)
            duration = float(row["elapsed_s"])
            valid_team = p >= 1 and p == int(row["requested_threads"])
            if not valid_team or key[0] not in solvers:
                raise ValueError(f"Unexpected solver or team size for N={n}.")
            if not math.isfinite(duration) or duration <= 0:
                raise ValueError(f"Invalid duration for N={n}.")
            round_times = times.setdefault(key, {})
            index = int(row["round"])
            if index in round_times:
                raise ValueError(f"Duplicate run for N={n}, {key}, round {index}.")
            round_times[index] = duration
        serial_teams = [p for solver, p in times if solver == "sequential"]
        if serial_teams != [1]:
            raise ValueError(f"Missing or invalid serial reference for N={n}.")
        if len(rounds) < 2 or any(set(samples) != rounds for samples in times.values()):
            raise ValueError(f"Quartiles need at least two complete rounds for N={n}.")
        serial = statistics.median(times[("sequential", 1)].values())
        order = sorted(times, key=lambda key: (solvers.index(key[0]), key[1]))
        for solver, p in order:
            samples = times[solver, p]
            median = statistics.median(samples.values())
            q1, _, q3 = statistics.quantiles(samples.values(), n=4, method="inclusive")
            speedup = serial / median
            summaries.append(Summary(n, solver, p, len(samples), median, q1, q3,
                                     q3 - q1, speedup, speedup / p))
    return summaries


def write_tables(output: Path, summaries: list[Summary]) -> None:
    with (output / "summary.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=asdict(summaries[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(asdict(row) for row in summaries)
    body = []
    for row in summaries:
        if row.solver == "sequential" and body:
            body.append(r"\addlinespace")
        mode = "Sequential" if row.solver == "sequential" else "OpenMP"
        body.append(
            f"{row.n:,} & {mode} & {row.threads} & {1000 * row.median_s:.1f} & "
            f"{1000 * row.iqr_s:.1f} & {row.speedup:.2f} & {100 * row.efficiency:.1f} "
            + r"\\"
        )
    table = [
        r"\begin{tabular}{rlrrrrr}",
        r"\toprule",
        r"$N$ & Solver & $p$ & Median (ms) & IQR (ms) & $S_p$ & $E_p$ (\%) \\",
        r"\midrule",
        *body,
        r"\bottomrule",
        r"\end{tabular}",
    ]
    (output / "timings.tex").write_text("\n".join(table) + "\n", encoding="utf-8")


def export_figure(figure: Figure, output: Path, name: str) -> None:
    """Write both views of the same figure, with reproducible PDF metadata."""
    figure.savefig(output / f"{name}.png", dpi=200)
    figure.savefig(output / f"{name}.pdf", metadata={"CreationDate": None})
    plt.close(figure)


def plot_speedup(output: Path, summaries: list[Summary]) -> None:
    figure, axis = plt.subplots(figsize=(7.2, 3.8), layout="constrained")
    parallel = [row for row in summaries if row.solver == "openmp"]
    if not parallel:
        raise ValueError("The figure requires an OpenMP configuration.")
    sizes = sorted({row.n for row in parallel})
    markers = ["o", "s", "^", "D"]
    colors = ["#13588A", "#A84715", "#307357", "#74468B"]
    for index, size in enumerate(sizes):
        rows = [row for row in parallel if row.n == size]
        axis.plot([row.threads for row in rows], [row.speedup for row in rows],
                  marker=markers[index % len(markers)], color=colors[index % len(colors)],
                  linewidth=2, markersize=6, label=f"N = {size:,}")
    axis.axhline(1, linestyle="--", color="#626970", linewidth=1,
                 label="Sequential solver at the same N")
    axis.set(title="OpenMP speedup by thread count",
             xlabel="Observed OpenMP team size", ylabel="Speedup relative to sequential (×)",
             xticks=sorted({row.threads for row in parallel}), ylim=(0, None))
    axis.set_axisbelow(True)
    axis.grid(axis="y", alpha=0.24)
    axis.spines[["top", "right"]].set_visible(False)
    counts = {row.runs for row in summaries}
    note = f"Medians of {counts.pop()} repetitions" if len(counts) == 1 else "Per-solver medians"
    axis.legend(frameon=False, loc="upper left", title=note, alignment="left")
    export_figure(figure, output, "speedup")


@dataclass(frozen=True, order=True)
class Evaluation:
    team: int
    repetition: int
    index: int


def read_arrivals(path: Path) -> tuple[int, dict[Evaluation, list[int]]]:
    """Check the case, complete teams and target totals before deriving spreads."""
    groups: dict[Evaluation, dict[int, tuple[int, int]]] = {}
    settings = set()
    case_fields = ("n", "k", "seed", "g", "epsilon", "dt", "initial_hash")
    with path.open(encoding="utf-8", newline="") as stream:
        for row in csv.DictReader(stream):
            settings.add(tuple(row[field] for field in case_fields))
            team = int(row["observed_threads"])
            key = Evaluation(team, int(row["round"]), int(row["evaluation"]))
            thread = int(row["thread_id"])
            count = int(row["targets"])
            finish = int(row["finish_ns"])
            workers = groups.setdefault(key, {})
            if team < 1 or team != int(row["requested_threads"]):
                raise ValueError(f"Unexpected observed team for {key}.")
            if thread in workers or not 0 <= thread < team or min(count, finish) < 0:
                raise ValueError(f"Invalid or duplicate thread record for {key}.")
            workers[thread] = (count, finish)
    if len(settings) != 1:
        raise ValueError("The arrival CSV must contain exactly one input case.")
    case = settings.pop()
    bodies, steps = int(case[0]), int(case[1])
    teams = {key.team for key in groups}
    repetitions = max(key.repetition for key in groups)
    expected = {
        Evaluation(team, repetition, index)
        for team in teams
        for repetition in range(1, repetitions + 1)
        for index in range(steps + 1)
    }
    if groups.keys() != expected:
        raise ValueError("Missing or unexpected repetition or force evaluation.")
    finishes = {}
    for key, workers in sorted(groups.items()):
        counts = [count for count, _ in workers.values()]
        if len(workers) != key.team or sum(counts) != bodies:
            raise ValueError(f"Incomplete workers or target total for {key}.")
        if bodies % key.team == 0 and any(count != bodies // key.team for count in counts):
            raise ValueError(f"Target counts differ in the equal-work comparison for {key}.")
        finishes[key] = [workers[tid][1] for tid in range(key.team)]
    return bodies, finishes


def write_spreads(output: Path, bodies: int, finishes: dict[Evaluation, list[int]]) -> None:
    """Compute one last-minus-first spread per evaluation, then summarize by team."""
    by_team: dict[int, list[float]] = {}
    with (output / "spreads.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["n", "threads", "round", "evaluation", "first_ns", "last_ns", "spread_ns"])
        for key, times in finishes.items():
            first, last = min(times), max(times)
            spread = last - first
            writer.writerow([bodies, key.team, key.repetition, key.index, first, last, spread])
            by_team.setdefault(key.team, []).append(spread / 1_000_000)
    with (output / "summary.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream, lineterminator="\n")
        writer.writerow(["n", "threads", "evaluations", "min_ms", "q1_ms", "median_ms",
                         "q3_ms", "max_ms"])
        for team, spreads in by_team.items():
            if len(spreads) == 1:
                q1 = median = q3 = spreads[0]
            else:
                q1, median, q3 = statistics.quantiles(spreads, n=4, method="inclusive")
            writer.writerow([bodies, team, len(spreads), min(spreads), q1, median,
                             q3, max(spreads)])


def plot_arrivals(
    output: Path, bodies: int, finishes: dict[Evaluation, list[int]],
    repetition: int, evaluation: int,
) -> None:
    teams = sorted({key.team for key in finishes})
    figure, panels = plt.subplots(1, len(teams), figsize=(7.5, 4.15), sharex=True, squeeze=False)
    figure.subplots_adjust(left=0.085, right=0.98, top=0.745, bottom=0.24, wspace=0.22)
    figure.suptitle("When OpenMP threads finish their work", fontsize=15, weight="semibold")
    figure.text(0.5, 0.875, f"N = {bodies:,}   ·   Repetition {repetition}   ·   Evaluation {evaluation}",
                ha="center", color="#38454d", fontsize=10)
    for axis, team in zip(panels[0], teams, strict=True):
        key = Evaluation(team, repetition, evaluation)
        if key not in finishes:
            raise ValueError(f"The selected example is missing: {key}.")
        times = [value / 1_000_000 for value in finishes[key]]
        axis.axvspan(min(times), max(times), color="#dce9ee", alpha=0.65,
                     label="First-to-last arrival spread")
        axis.scatter(times, range(team), color="#155777", s=34, zorder=3,
                     label="Thread finish before the barrier")
        axis.set(title=f"{team} threads", yticks=range(team), ylim=(-0.65, team - 0.35))
        axis.grid(axis="x", color="#c5cdd1", alpha=0.65)
        axis.spines[["top", "right"]].set_visible(False)
        axis.invert_yaxis()
    panels[0, 0].set_xlim(left=0)
    panels[0, 0].set_ylabel("OpenMP thread ID")
    figure.supxlabel("Time from force-evaluation start (ms)", y=0.10, fontsize=11)
    handles, labels = panels[0, 0].get_legend_handles_labels()
    figure.legend(handles[::-1], labels[::-1], loc="lower center", frameon=False, ncols=2)
    export_figure(figure, output, "thread-finishes")


def record_figure_inputs(
    output: Path, speedup_csv: Path, arrivals_csv: Path, repetition: int, evaluation: int
) -> None:
    sources = {
        "benchmarks/analyze.py": Path(__file__),
        "unprofiled_timing_csv": speedup_csv,
        "thread_arrival_csv": arrivals_csv,
    }
    provenance = {
        "python": platform.python_version(),
        "matplotlib": matplotlib.__version__,
        "example_repetition": repetition,
        "example_evaluation": evaluation,
        "sha256": {name: hashlib.sha256(path.read_bytes()).hexdigest()
                   for name, path in sources.items()},
    }
    (output / "figures.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path, help="CSV written by run_campaign.py")
    parser.add_argument("--output", type=Path, help="Defaults to the input directory")
    parser.add_argument("--arrivals", type=Path, help="Raw CSV from the arrivals mode")
    parser.add_argument("--arrivals-output", type=Path, help="Defaults to the arrival CSV directory")
    parser.add_argument("--example-round", type=int, default=3)
    parser.add_argument("--example-evaluation", type=int, default=6)
    args = parser.parse_args()
    with args.raw.open(encoding="utf-8", newline="") as stream:
        summaries = summarize(list(csv.DictReader(stream)))
    destination = args.output or args.raw.parent
    if args.arrivals:
        arrival_output = args.arrivals_output or args.arrivals.parent
        if arrival_output.resolve() == destination.resolve():
            parser.error("Choose separate output directories for the two summary tables.")
        bodies, finishes = read_arrivals(args.arrivals)
        arrival_output.mkdir(parents=True, exist_ok=True)
        plot_arrivals(arrival_output, bodies, finishes, args.example_round, args.example_evaluation)
        write_spreads(arrival_output, bodies, finishes)
    destination.mkdir(parents=True, exist_ok=True)
    write_tables(destination, summaries)
    plot_speedup(destination, summaries)
    if args.arrivals:
        record_figure_inputs(arrival_output, args.raw, args.arrivals,
                             args.example_round, args.example_evaluation)


if __name__ == "__main__":
    main()
