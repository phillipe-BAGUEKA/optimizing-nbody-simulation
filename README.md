# Optimizing N-Body Simulation

Implementation, profiling, and optimization of direct 3D N-body simulations in C++20 with OpenMP.

**Status:** project framing. The repository currently contains planning documents only. The simulator, benchmarks, interface, and report have not been implemented or validated yet.

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
- Compute all accelerations from the previous state before updating velocities and positions.
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

Initial conditions, physical parameters, numerical settings, thread counts, compiler settings, and machine information will be recorded. Performance comparisons will use identical inputs and simulation settings. Visualization and trajectory export will be outside the timed simulation region.

No benchmark results or performance claims are available yet.

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
