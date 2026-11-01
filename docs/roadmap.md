# Learning and implementation roadmap

Planning date: 2026-11-01.

This roadmap is a working plan. An item is complete only when the deliverable exists and the relevant understanding has been demonstrated. Familiarity reported by the learner is a starting point, not a fabricated assessment.

## Starting point

The learner already understands basic threads and std::jthread and has introductory OpenMP experience. Build on this knowledge. Use brief, applied checks of read/write dependencies, synchronization, and scheduling, and explain unfamiliar concepts or demonstrated gaps as needed. Do not repeat a generic beginner introduction to threads.

Codex handles local cloning, environment inspection, and setup commands directly; explanations of build tools remain brief.

## Current status

- [x] Private GitHub repository created by the learner.
- [x] Repository access verified and initial project framing added.
- [ ] Local clone and Windows/MSYS2 UCRT64 toolchain verified.
- [ ] C++20/OpenMP build smoke check completed on the learner's computer.
- [ ] Physical model and numerical choices explained and agreed.
- [ ] Sequential or parallel simulator implemented.
- [ ] Numerical or performance results produced.

## Milestone 0 - Practical setup

Keep introductory explanations of compilers and build systems brief.

1. Clone the repository locally.
2. Check the existing UCRT64 GCC, Git, CMake, Ninja, and debugger; install only missing tools.
3. Record the CPU model, physical core count, logical processor count, OS, and compiler version.
4. Introduce a minimal CMake project and a small OpenMP build smoke check.
5. Explain how to configure, build, run, and use the debugger.
6. Introduce Debug versus Release and explain why meaningful performance measurements need an appropriate optimized build.

Exit criteria: the local checkout builds and runs the smoke check successfully, the toolchain is identified, and concise reproduction commands are documented. No gravitational solver is required for this milestone.

## Milestone 1 - Physics, numerical method, and complexity

### Physical model

- Scalars, vectors, displacement, distance, and units.
- Newtonian gravitational force and acceleration.
- Superposition of contributions from the other bodies.
- Softening: mathematical expression, numerical motivation, and effect on the physical model.
- Initial conditions: masses, positions, velocities, units, and reference frame.
- Fixed-step numerical integration; understand the candidate method before selecting it.
- Calculate all accelerations from the same old state before any state update.

### Complexity

- Input size, elementary operations, asymptotic upper bounds O, and tight bounds Theta where justified.
- O(1), O(log N), O(N), O(N log N), and O(N^2) on small concrete examples.
- Count operations from loop bounds rather than guessing from the number of loops.
- For the direct directed interaction loop: N(N-1) contributions per time step.
- Distinguish this from N(N-1)/2 unordered pairs and the implementation implications of exploiting symmetry.
- With S time steps, the straightforward direct solver performs Theta(S*N^2) interaction work.
- Core state can use Theta(N) storage without materializing an N-by-N interaction matrix.
- Retaining every trajectory sample can require Theta(S*N) storage; output and analysis storage must be accounted for separately.
- Distinguish total work, memory use, parallel elapsed time, and fixed hardware resources.
- Explain why using p threads or SIMD does not by itself change quadratic total work.
- Understand Barnes-Hut conceptually as a different approximate algorithm; do not substitute it for the assignment's direct solver.

Learner tasks: work through small two- or three-body cases, derive the acceleration expression, count loop operations, and identify which computations may overlap.

Exit criteria: the learner can reconstruct one time step, justify the read/write dependencies, and explain time and space complexity. Produce report v0 with the agreed model, scope, and methodology; do not insert unmeasured results.

## Milestone 2 - Initial conditions and sequential C++ reference

1. Design small, explicit C++ data types and interfaces.
2. Practice const correctness, references, value semantics, contiguous storage, RAII, and clear ownership.
3. Build hand-designed validation cases, followed by a documented reproducible input generator.
4. Choose and record provisional configurations; no numerical assignment variant was supplied in the available screenshots.
5. Preserve identical initial states for all implementation comparisons.
6. Implement the direct acceleration calculation and the selected fixed-step integrator.
7. Add a command-line entry point, configuration checks, and optional output.
8. Validate isolated-body motion, pair interactions, appropriate symmetries, and numerical finiteness.
9. Study numerical convergence on controlled cases; expected physics must be consistent with the softening and integration scheme.

Learner-owned core: vector operations as needed, acceleration accumulation, a simulation step, and selected meaningful tests.

Exit criteria: a readable sequential reference with independent correctness checks. The learner explains the code, the state transitions, and the operation/storage counts. Update the LaTeX report with implementation and verification.

## Milestone 3 - Baseline OpenMP

1. Analyze the sequential loop's reads and writes before adding directives.
2. Partition target bodies between threads; old-state inputs remain read-only during acceleration computation.
3. Understand shared/private variables and per-body local accumulation.
4. Preserve completion of the acceleration phase before position/velocity updates and completion of updates before the next time step.
5. Identify data races, synchronization needs, and the consequences of removing a required barrier.
6. Understand and choose a baseline scheduling policy.
7. Set and record the actual thread count.
8. Compare positions and velocities against the sequential reference at several thread counts using predefined absolute/relative tolerances.

Introduce a minimal benchmark protocol here: identical inputs and settings, optimized builds, timing boundaries, repeated runs, and raw measurement output. Separate numerical diagnostics and trajectory export from the timed region.

Exit criteria: a verified OpenMP baseline and initial measurements. The learner can justify the parallel decomposition and necessary synchronization.

## Milestone 4 - Profiling and targeted optimization

1. Choose a profile tool compatible with the actual CPU, OS, compiler, and runtime.
2. Distinguish total timing, hotspots, waiting, and a demonstrated limiting resource.
3. Capture a baseline profile and formulate a specific testable hypothesis.
4. Select an optimization supported by the evidence.
5. Change one main factor at a time, validate correctness, repeat measurements, and compare profiles.
6. Document unsuccessful attempts and the limits of the evidence.

Candidates, not predetermined requirements or guaranteed improvements:

- AoS versus SoA data layout.
- SIMD and floating-point reductions.
- Loop transformations and blocking for data reuse.
- Avoidance of redundant arithmetic.
- Scheduling and parallel-region organization.
- Thread affinity after understanding the hardware.
- Optional direct pair-symmetry experiment: reduced arithmetic versus shared-write/synchronization costs.

All assessed implementations retain the direct algorithm, physical model, fixed time step, and agreed integrator. Convergence experiments are separate from performance comparisons.

Exit criteria: at least one complete chain from observed issue to hypothesis, change, numerical validation, and measured effect. Keep the sequential reference and baseline OpenMP implementation available.

## Milestone 5 - Full experimental study

1. Define a tractable experiment matrix over N and thread count p; keep all other settings fixed within each comparison.
2. Explain strong scaling: fixed problem size, varying available execution resources.
3. Record inputs, compiler/options, machine, actual thread counts, timings, and code revision.
4. Repeat measurements, report a chosen central statistic and variability, and retain raw data.
5. Compute speedup and parallel efficiency relative to the sequential reference.
6. Also compare optimized versus baseline OpenMP at the same p, so optimization gain is distinguished from thread scaling.
7. Where useful, compare the optimized implementation at one thread versus p threads.
8. Analyze observed scaling against the operation count; do not claim that a timing curve alone proves asymptotic complexity.
9. Regenerate tables and plots from raw data using Python/Jupyter.
10. Explain plateaus and unexpected results with evidence, labeling unconfirmed hypotheses.

Planned tools: pandas, Matplotlib/Seaborn for report figures, and Plotly when interaction helps analysis.

Exit criteria: a reproducible notebook and documented experiment command, raw results, figures, and defensible conclusions. Update the report.

## Milestone 6 - Visualization and dashboard

The C++ engine remains the source of simulated trajectories.

1. Start with playback of exported trajectories.
2. Use React and TypeScript for controls/dashboard and Three.js for 3D rendering.
3. Add camera control, play/pause, a timeline, trails, and body selection.
4. Display a selected body's position, velocity, and acceleration over time.
5. Add a small educational scene showing force contributions and their resultant.
6. Display previously measured performance and numerical-validation results.
7. If the core and experiments are complete, add controls to modify initial conditions and rerun the same native C++ engine through an explicit local integration layer.
8. Keep animation playback speed separate from the physical time step.
9. Keep rendering, export, and educational instrumentation out of benchmark timings.

Optional data extension: obtain geometric state vectors from NASA/JPL Horizons at one common epoch, frame, origin, and unit system, with suitable mass/GM data. Preserve provenance and acknowledge differences from the full ephemeris model. A documented Plummer-like initial-condition generator is another possible extension; arbitrary random velocities do not establish an equilibrium cluster.

Exit criteria: a working demonstration that uses real output from the implemented solver. Live reruns are a stretch goal, not a prerequisite for core assignment completion.

## Milestone 7 - Report, packaging, and defense

Report authoring is continuous from milestone 1, not postponed until this milestone.

1. Maintain LaTeX source, references, generated figures, and versioned PDF releases.
2. Present requirements, model, design, sequential reference, OpenMP decomposition, validation, profiling, optimization, and experimental results.
3. Explain limitations, failed hypotheses, and future work.
4. Keep the main report close to the requested 12-18 pages.
5. Verify that the README commands work from a clean checkout in a supported environment.
6. Prepare the single submission archive with PDF, source, build/configuration files, raw experimental data, and input-generation or acquisition instructions.
7. Prepare an approximately 10-minute presentation and practice the approximately 5-minute question session.
8. Practice explaining important code, data dependencies, complexity, measurement choices, and optimization evidence without relying on the assistant.

Working deadline: 14 December 2026 pending clarification of the inconsistent 14/16 December dates in the supplied material. Reserve time before the deadline for reproduction checks and submission.

## Continuous learning record

Track each substantial concept as:

- introduced;
- practiced;
- demonstrated independently;
- requiring further explanation.

Use observed answers and implementations as evidence. Never infer mastery from elapsed time, generated code, or an unattempted exercise.

## Scope priorities

Required: correct direct 3D simulation, sequential and two OpenMP versions, profiling evidence, validation, experiments, and the report.

Planned presentation enhancement: trajectory viewer and dashboard.

Optional after the required work is secure: richer scientific scenarios, interactive reruns, and advanced direct-kernel experiments.

Later extensions: CUDA, Barnes-Hut, distributed execution. They must not displace the assessed CPU/OpenMP study.
