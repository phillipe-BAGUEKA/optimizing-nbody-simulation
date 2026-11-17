# First OpenMP baseline campaign

The experiment varies the team size for a fixed problem (strong scaling). It uses the direct target-by-target solver already in the project. The two-body demonstration is a separate arithmetic example. Small tests and the tolerances are described in [model and validation](model-and-validation.md).

## Follow one step in the demo

Run `nbody_demo --solver openmp --case two-body --steps 1 --dt 0.5 --threads 4 --diagnostics`. The call path is:

1. `main` calls `parse_options`, disables dynamic team adjustment and requests four threads. `make_two_body_example` creates masses 2 and 10, separated by 3 along y, with zero velocities. The demo supplies G = 1 and softening 4. [Entry point](../src/main.cpp) · [Example](../include/nbody/examples.hpp)
2. `simulate_velocity_verlet` enters `run_velocity_verlet`. Its first `evaluate_accelerations` calls `detail::fill_accelerations_openmp` on the original positions. The static outer loop assigns the two targets. For each target, `acceleration_for_target` visits the other source and accumulates `gravitational_contribution`. The y accelerations are 0.24 and −0.048. [Integrator](../src/simulation.cpp) · [Gravity](../src/gravity.cpp)
3. The first sequential update moves the y positions to 0.03 and 2.994. A second complete force evaluation uses those new positions. The next loop updates both velocities with the mean of the old and new accelerations, giving y velocities 0.12005194016285471 and −0.024010388032570943. The buffers swap and `main` prints the final state. [Position and velocity updates](../src/simulation.cpp)

Target iteration zero records `omp_get_num_threads()` inside the actual last force region. The four team members encounter its completion barrier. Two receive one target apiece and two have an empty iteration range. The diagnostic measures membership, not useful iterations executed.

## Inputs, machine and timing interval

The assignment's individual physical parameters are still pending. For this provisional workload, `make_initial_state` seeds `std::mt19937_64` with 20261117 and converts its high 53 bits into a uniform double in [0, 1). For each body it draws mass in [0.5, 1.5), three position components in [−1, 1), then three velocity components in [−0.1, 0.1). Units are normalized, with G = 1, epsilon = 0.05, dt = 0.0005 and K = 12. The distribution is for reproducible timing. Physical equilibrium and convergence have yet to be tested.

The machine reported Windows 11 Home 10.0.26300, an Intel Core 7 150U, 10 physical cores and 12 logical processors. The active power plan was Balanced. The run inherited no `OMP_*` or `GOMP_*` settings. Windows scheduled the threads. Background load, affinity, CPU frequency and core placement were uncontrolled. GCC 16.2.0 (MSYS2 UCRT64 Rev4), libgomp/OpenMP 5.2, CMake 4.4.4 and Ninja 1.13.2 supplied the build. Release used `-O3 -DNDEBUG -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -fopenmp`.

The measured checkout had local changes on top of `9bd871621b9c47608139fa8d685ff3d83624a103`. The metadata records the dirty paths, SHA-256 digests of the measured source files and binary, the actual compiler commands and OS settings. Restoring that commit alone cannot reproduce this binary. [Campaign metadata](../results/baseline/metadata.json)

For each N, one process generates a master input and computes a checked sequential reference. Each configuration runs once on a fresh copy for warm-up, preflight comparison and team observation in the force loop. The program rejects any difference in mass, non-finite component, or position/velocity error above `1e-12 + 1e-12 * max(abs(actual), abs(expected))`. An unavailable team or a count different from the request also stops the campaign. The recorded team size comes from that corresponding control. The genuine serial solver takes the ordinary target loop.

Each measured run starts with a fresh copy. In `measure_velocity_verlet`, both acceleration buffers and input validation precede the first `steady_clock` read. The interval starts at the initial force evaluation and ends after the last velocity update and buffer swap. It includes 13 full evaluations, both state-update loops, and the OpenMP entry/join on every evaluation. The same pair contribution and its numerical guard run in both solvers. The checked control performs intermediate validation scans. Timed runs omit those scans and team diagnostics, then check the final state, compare to the reference, fingerprint and write after the clock stops. [Harness](../benchmarks/benchmark.cpp) · [Shared integrator](../src/simulation.cpp)

## Pilot and measurement order

The pilot used one measured run after warm-up at each configuration. The numbers below are milliseconds. [Pilot CSV](../results/pilot/raw.csv)

| Bodies | Sequential | OpenMP 4 | OpenMP 12 |
| ---: | ---: | ---: | ---: |
| 512 | 10.4 | 8.4 | 6.8 |
| 1,536 | 98.9 | 51.3 | 42.1 |
| 3,072 | 486.7 | 214.7 | 131.6 |

The latter two sizes kept even the fastest pilot run above 40 ms while allowing six rounds. They gave 30,650,880 and 122,643,456 directed contributions per simulation. The first round ran serial, OpenMP 1, 2, 4, 8 and 12 in that order. Each next round started one configuration later, wrapping around. Each configuration occupied every order position once. The 1,536-body campaign completed before the 3,072-body campaign.

## Timing results

Each entry below is the median of six unprofiled runs. IQR is the time at the 75th percentile minus the time at the 25th percentile, using inclusive linear interpolation. Speedup is the serial median divided by the OpenMP median at the same N. Efficiency divides this speedup by the observed team size. The serial row is the reference. [Raw timings](../results/baseline/raw.csv) · [Precise summary with quartiles](../results/baseline/summary.csv)

| Solver / team | 1,536 median / IQR (ms) | Speedup | Efficiency | 3,072 median / IQR (ms) | Speedup | Efficiency |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Sequential | 115.6 / 7.4 | 1.00 | 100.0% | 479.7 / 9.3 | 1.00 | 100.0% |
| OpenMP 1 | 120.9 / 4.5 | 0.96 | 95.7% | 484.1 / 25.5 | 0.99 | 99.1% |
| OpenMP 2 | 68.2 / 5.2 | 1.70 | 84.8% | 265.8 / 25.1 | 1.80 | 90.2% |
| OpenMP 4 | 53.9 / 1.8 | 2.15 | 53.7% | 219.4 / 24.5 | 2.19 | 54.6% |
| OpenMP 8 | 43.6 / 2.3 | 2.65 | 33.1% | 164.8 / 8.4 | 2.91 | 36.4% |
| OpenMP 12 | 39.4 / 1.7 | 2.94 | 24.5% | 135.3 / 1.3 | 3.54 | 29.5% |

OpenMP at one thread was 5.2 ms and 4.4 ms slower in the two medians. Variability, especially the 25.5 ms IQR in the larger case, prevents interpreting those differences as a precise estimate of parallel-region overhead. Median speedup increased at each tested team size. At 12 threads the larger case had higher efficiency. Six rounds on one machine do not isolate a cause.

Every run records its initial and final fingerprints. The two final fingerprints were `cd6c1a4f71805c41` and `c0f767c838042956`. These identify the archived states. Component-wise comparison determined acceptance. Each final-state file contains mass, position and velocity for every indexed body from the last run of its size. [1,536 bodies](../results/baseline/final-1536.csv) · [3,072 bodies](../results/baseline/final-3072.csv)

## Sampling profile

The capture used the same large input and OpenMP team of four, repeated for 60 fresh 12-step batches. A separate Release build added `-g` to the effective flags above. The MSYS2 Cygwin profiler 3.6.10 attached about half a second after launch at a requested 100 Hz. Launching directly under the profiler sometimes failed during transient DLL mapping with `read_child: failed`. Late attachment let it observe the initialized workload. The profile includes batch setup, copies, checks and hashes after attachment. Times in the table come from the standalone campaign.

The profiler reads the instruction pointer of each Windows thread it samples and pools hits by module and four-byte address bin. It can encounter threads in waits. Windows ASLR relocated the executable, so interpreting the runtime addresses directly with plain `gprof` gave an empty table. The exporter matches the runtime and linked `.text` extents, translates each bin start as `linked_text_start + (runtime_pc - runtime_text_start)` and resolves the inline chain with UCRT64 `addr2line -afiC`. Optimized instructions and four-byte bins limit per-line attribution. [Profiler manual](https://cygwin.com/cygwin-ug-net/profiler.html) · [Upstream sampling implementation](https://sourceware.org/pipermail/cygwin-cvs/2021q3/014898.html)

The capture recorded 10,471 mapped hits, including 4,916 in the executable and 5,552 in `ntdll.dll`. Of the executable hits, 4,914 (99.96%) have the outlined OpenMP target loop in their inline chain and 4,912 include `acceleration_for_target`. For example, 961 hits at linked bin `0x140002fd4` resolve through vector multiplication, `gravitational_contribution`, `acceleration_for_target` and the OpenMP loop. This share uses executable hits as its denominator. DLL samples include other process threads and cannot be converted into the fraction of time spent waiting at a barrier. [Thread and module events](../results/profile/events.txt) · [Symbols per bin](../results/profile/symbols.csv) · [Native executable histogram](../results/profile/executable.gmon)

During a 3.432 s wall-time window of this same PID, two Windows snapshots show four threads accumulating 2.688, 2.453, 2.641 and 2.688 CPU seconds. All seven threads present in both snapshots accumulated 10.484 CPU seconds in total. Concurrent work adds CPU seconds on multiple threads while the wall clock advances once. These are separate OS counters. The profiler histogram merges thread identities and cannot assign a bin to an individual thread. All 60 final hashes match the archived large case. [Capture configuration and CPU window](../results/profile/capture.json) · [Batch outputs](../results/profile/batches.txt)

The capture locates the force loop. Distinguishing arithmetic latency from cache-related stalls requires measurements such as divider-active cycles alongside data-cache miss/stall counters on the same workload and core type. A separate test for losses at larger teams could measure each worker's final target completion and arrival at the barrier on the same inputs.

## Reproduce on this Windows installation

Run the following in PowerShell from the repository root. The campaign launcher uses Python 3.11 or later. The local Anaconda interpreter has Matplotlib 3.9.2 and psutil 5.9.0. The raw runners require a new or empty destination, so these commands use the ignored build directory for repeat runs. To redraw the published figure and table, give the analysis script the archived raw CSV.

```powershell
$env:Path = 'C:\msys64\ucrt64\bin;' + $env:Path
$analysisPython = Join-Path $env:USERPROFILE 'anaconda3\python.exe'
cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
cmake --preset ucrt64-release
cmake --build --preset ucrt64-release
ctest --preset ucrt64-release
.\build\ucrt64-release\nbody_demo.exe --solver openmp --case two-body --steps 1 --dt 0.5 --threads 4 --diagnostics
& $analysisPython benchmarks/run_campaign.py --sizes 512 1536 3072 --threads 4 12 --steps 12 --seed 20261117 --repetitions 1 --no-states --output build/repeat-pilot
& $analysisPython benchmarks/run_campaign.py --sizes 1536 3072 --threads 1 2 4 8 12 --steps 12 --seed 20261117 --repetitions 6 --output build/repeat-baseline
& $analysisPython benchmarks/analyze.py build/repeat-baseline/raw.csv
& $analysisPython benchmarks/analyze.py results/baseline/raw.csv
cmake --preset ucrt64-release -B build/ucrt64-profile '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -g' -DBUILD_TESTING=OFF
cmake --build build/ucrt64-profile --target nbody_benchmark
& $analysisPython benchmarks/profile_openmp.py --output build/repeat-profile
& $analysisPython benchmarks/profile_openmp.py --replay results/profile --output build/replayed-symbols
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=report report/main.tex
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=report report/main.tex
```

The profile script runs `nbody_benchmark.exe profile 3072 12 20261117 60 4`, then `profiler.exe -v -e -s 100 -p -<WindowsPID>`. The Windows PID varies on each run. The script records HEAD, dirty paths, hashes of the source files and the executable before attachment and verifies the files again after capture. GNU `objcopy --only-keep-debug` extracts the linked section addresses and DWARF symbols. The archived histogram and symbol file are both checksummed. The replay command resolves the original bins directly from these two files, including on a checkout where the original build directory is gone. [Detached symbols](../results/profile/executable.debug) · [Capture and symbol export](../benchmarks/profile_openmp.py) · [Figure generator](../benchmarks/analyze.py)
