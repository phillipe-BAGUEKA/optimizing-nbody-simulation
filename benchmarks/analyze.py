"""Summarize unprofiled campaign rows and plot speedup relative to the serial solver."""

import argparse
import csv
from dataclasses import asdict, dataclass
import math
from pathlib import Path
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


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


def summarize(rows: list[dict[str, str]]) -> list[Summary]:
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
            if not valid_team or key[0] not in {"sequential", "openmp"}:
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
        order = sorted(times, key=lambda key: (key[0] == "openmp", key[1]))
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
    axis.axhline(1, linestyle="--", color="#626970", linewidth=1, label="Sequential reference")
    axis.set(xlabel="Observed OpenMP team size", ylabel="Speedup relative to sequential",
             xticks=sorted({row.threads for row in parallel}), ylim=(0, None))
    axis.set_axisbelow(True)
    axis.grid(axis="y", alpha=0.24)
    axis.spines[["top", "right"]].set_visible(False)
    axis.legend(frameon=False, loc="upper left")
    figure.savefig(output / "speedup.png", dpi=200)
    plt.close(figure)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("raw", type=Path, help="CSV written by run_campaign.py")
    parser.add_argument("--output", type=Path, help="Defaults to the input directory")
    args = parser.parse_args()
    with args.raw.open(encoding="utf-8", newline="") as stream:
        summaries = summarize(list(csv.DictReader(stream)))
    destination = args.output or args.raw.parent
    destination.mkdir(parents=True, exist_ok=True)
    write_tables(destination, summaries)
    plot_speedup(destination, summaries)


if __name__ == "__main__":
    main()
