# Optimizing N-Body Simulation

Implementation, profiling, and optimization of direct 3D N-body simulations in C++20 with OpenMP.

The direct sequential and baseline OpenMP solvers share a Velocity Verlet integrator. Debug and Release tests cover physical references and solver equivalence. The first performance campaign measures two provisional system sizes on the local Windows machine. Definitive assignment inputs, convergence, an optimized version and the interface remain future work. [Baseline results and reproduction](docs/baseline-performance.md)

## Objectives

- Develop a solid understanding of parallel programming, modern C++, and algorithmic complexity.
- Understand the gravitational model and numerical integration sufficiently to implement and validate it.
- Compare a sequential reference, an initial OpenMP implementation, and a profiling-driven optimized OpenMP implementation.
- Explain performance improvements and limitations using reproducible measurements.
- Build an interactive visualization of trajectories produced by the C++ engine.
- Write the semester report progressively in LaTeX and deliver the compiled PDF.

## Assignment constraints

- Model a three-dimensional system with mass, position, and velocity for each body.
- Compute gravitational accelerations using a documented softening parameter.
- Evaluate all accelerations from a complete same-time position state. In Velocity Verlet, compute the initial accelerations, update all positions, recompute all accelerations, then update all velocities.
- Use a fixed time step and the same integration scheme in all compared implementations.
- Keep the direct O(N^2) interaction algorithm in the assessed optimization work.
- Validate parallel positions and velocities against the sequential reference using a predefined numerical tolerance.
- Demonstrate the sequence: measurement, profiling, identified bottleneck, targeted optimization, and repeated measurement.
- Report speedup and parallel efficiency relative to the sequential reference.

CUDA and Barnes-Hut are possible future extensions. They are outside the initial implementation plan; Barnes-Hut does not replace the required direct algorithm.

## Roadmap

The [detailed roadmap](docs/roadmap.md) defines the learning tasks, implementation work, and completion criteria for each milestone.

| Milestone | Deliverable |
| --- | --- |
| 0 | Repository and a working local C++20/OpenMP build environment |
| 1 | Physical model, numerical method, and complexity analysis |
| 2 | Reproducible initial conditions and a verified sequential reference |
| 3 | A verified baseline OpenMP implementation |
| 4 | Profiling evidence and targeted, validated optimization |
| 5 | Reproducible experiments, notebooks, tables, and plots |
| 6 | Interactive visualization and a results dashboard |
| 7 | Final report, reproducible submission archive, and defense preparation |

The report and learning record evolve throughout the milestones.

## Learning workflow

For each substantial concept: explanation, prediction, a focused learner implementation, verification, and explanation in the learner's own words.

Core exercises are reserved for the learner. Codex may implement agreed supporting work or a bounded code change when faster progress is requested. Generated code is then reviewed, predicted, tested, and explained.

Conversation and coaching are in French. Code identifiers, public technical documentation, and the report are in English.

## Reproducibility

The first campaign used a fixed seed, copied initial states, 12 steps, six measured rounds and observed teams of 1, 2, 4, 8 and 12. For 3,072 bodies the median fell from 479.7 ms with the sequential solver to 135.3 ms with 12 OpenMP threads. The raw runs, build fingerprints, dispersion, plotting script and separate sampling profile are documented in the [campaign protocol](docs/baseline-performance.md).

## Milestone 0 build check

The original `openmp_smoke_check` executable is retained alongside the simulator. Run the following commands from an MSYS2 UCRT64 shell:

```bash
cd /c/Users/loicp/Documents/I3/02-C++/000-PROJECT/n_body

cmake --preset ucrt64-debug
cmake --build --preset ucrt64-debug
ctest --preset ucrt64-debug
./build/ucrt64-debug/openmp_smoke_check.exe
```

Use the Release preset to verify an optimized build:

```bash
cmake --preset ucrt64-release
cmake --build --preset ucrt64-release
ctest --preset ucrt64-release
./build/ucrt64-release/openmp_smoke_check.exe
```

Debug keeps information useful to GDB. Release enables compiler optimizations and is the appropriate starting point for later performance measurements.

Start an interactive debugging session with the Debug executable:

```bash
gdb ./build/ucrt64-debug/openmp_smoke_check.exe
```

The verified local toolchain and machine details are recorded in [docs/development-environment.md](docs/development-environment.md).

## Simulation and validation

`nbody::Vec3` provides addition, subtraction, scalar multiplication, accumulation, and norms. `Body` stores mass, position, and velocity. Bodies and accelerations occupy separate vectors. The two solvers share the contribution calculation and source iteration order; a single Velocity Verlet implementation selects the solver through an enum.

After building, run the supplied two-body arithmetic reference and the four-body example:

```bash
./build/ucrt64-release/nbody_demo.exe --solver sequential --case two-body --steps 1 --dt 0.5
./build/ucrt64-release/nbody_demo.exe --solver openmp --case two-body --steps 1 --dt 0.5 --threads 4 --diagnostics
./build/ucrt64-release/nbody_demo.exe --solver sequential --case multi-body --steps 25 --dt 0.01
./build/ucrt64-release/nbody_demo.exe --solver openmp --case multi-body --steps 25 --dt 0.01 --threads 4 --diagnostics
```

`--help` lists options. Defaults are sequential, two-body, 10 steps, `dt=0.01`, and one requested OpenMP thread. The optional `--diagnostics` reports the team size observed inside the last force evaluation. It is disabled by default. `--threads` is accepted only in OpenMP mode; the runtime can limit the actual team size. Output uses 17 significant digits and includes parameters and initial/final states.

`ctest --preset ucrt64-debug` and `ctest --preset ucrt64-release` execute the six original checks and two benchmark integration checks. Tests remain active in Release. To see the observed 1/2/4-thread teams, use `ctest --preset ucrt64-release -R simulation_tests -V`. The test targets are created when `BUILD_TESTING=ON` (the default).

See [model, inputs, contracts and validation](docs/model-and-validation.md) for the complete provisional cases, units, exact reference constants, input validation, and numerical limitations. With `K` steps, the integrator performs `K + 1` complete force evaluations, including initialization, and uses linear storage without an interaction matrix.

For the report, run from the repository root with an existing LaTeX installation:

```bash
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=report report/main.tex
```

The [compiled report](report/main.pdf) covers the model, verification, measured strong scaling and the baseline profile. The final report target is 12–18 main-text pages. Recorded-trajectory playback and progressive state reception are planned.

## Submission

The planned Moodle submission is a single archive containing:

- the PDF report, approximately 12-18 pages of main text;
- complete source code and build/configuration files;
- clear build and execution instructions;
- raw experimental data used for the tables and plots;
- input data or instructions for generating or obtaining it.

The working submission target is **14 December 2026**, the earlier date in the supplied assignment material. The Moodle entry and Lecture 1 mention 16 December; the definitive date and topic-registration status still need confirmation.

## References

- [NVIDIA: Fast N-Body Simulation with CUDA](https://developer.nvidia.com/gpugems/gpugems3/part-v-physics-simulation/chapter-31-fast-n-body-simulation-cuda)
- [Intel: Direct N-body Simulation](https://www.intel.com/content/www/us/en/developer/articles/technical/direct-n-body-simulation.html)
- [OpenMP specifications](https://www.openmp.org/specifications/)
- [NASA/JPL Horizons manual](https://ssd.jpl.nasa.gov/horizons/manual.html)
- [REBOUND: a self-gravitating Plummer sphere](https://rebound.hanno-rein.de/c_examples/selfgravity_plummer/)
