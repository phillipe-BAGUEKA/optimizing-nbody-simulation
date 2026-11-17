# Direct solver and Velocity Verlet baseline

Verified locally on 2026-11-17 with the MSYS2 UCRT64 GCC 16.2.0 toolchain. The original OpenMP smoke check and Vec3 norm tests are retained. For performance measurements of this model, see the [baseline campaign](baseline-performance.md).

## State and physical model

`std::vector<Body>` holds positive masses, positions and velocities. Two separate `std::vector<Vec3>` buffers hold current and next accelerations. For each target `i`, visit every source `j` in increasing index order and exclude `i == j`:

```text
displacement = position[j] - position[i]
q = displacement.squared_norm() + epsilon * epsilon
contribution = displacement * (G * mass[j] / (q * sqrt(q)))
```

The target mass does not enter its acceleration. All masses and positions are read-only throughout an evaluation. Softening is strictly positive, so coincident bodies have a zero displacement contribution for moderate inputs without a zero-distance singularity.

## Provisional examples and units

These are explicit educational inputs, not the teacher's missing individual assignment variant and not a claim of astrophysical equilibrium. Use normalized units: mass `M`, length `L`, time `T`, velocity `L/T`, acceleration `L/T^2`, and `G` in `L^3/(M T^2)`. Both examples use `G=1`; no conversion to SI or ephemeris data is implied.

The `two-body` example uses `epsilon=4 L`:

| Body | Mass (M) | Position (L) | Velocity (L/T) |
| --- | --- | --- | --- |
| A / 0 | 2 | (0, 0, 0) | (0, 0, 0) |
| B / 1 | 10 | (0, 3, 0) | (0, 0, 0) |

This softening value simplifies arithmetic and is not a scientific recommendation. Initially `q=25` and `q*sqrt(q)=125`, giving `a_A=(0,0.24,0)` and `a_B=(0,-0.048,0)`.

The `multi-body` example uses `epsilon=0.1 L` and four non-aligned bodies:

| Body | Mass (M) | Position (L) | Velocity (L/T) |
| --- | --- | --- | --- |
| 0 | 2 | (-1, 0, 0.5) | (0.05, 0.10, -0.02) |
| 1 | 1.5 | (1, 0.5, -0.5) | (-0.10, 0.02, 0.03) |
| 2 | 0.75 | (0, -1, 1) | (0.04, -0.03, 0.08) |
| 3 | 3 | (0.5, 1.5, 0.25) | (-0.02, 0.06, -0.04) |

Demonstrations use one step of `dt=0.5 T` for the arithmetic reference and 25 steps of `dt=0.01 T` for the four-body case. These step sizes are not validated for general scientific use.

## Integration and work counts

Calculate the initial acceleration array once. For each step:

1. Update **all** positions using old velocities and accelerations: `r += v*dt + a*(0.5*dt*dt)`.
2. Evaluate all new accelerations from this complete new position state.
3. Update all velocities: `v += (a + a_new)*(0.5*dt)`.
4. Swap the acceleration buffers so the new values become current.

The time, position-update and velocity-update loops are sequential in both modes. For `K` steps there are exactly `K+1` full acceleration evaluations, even when `K=0`. Each evaluation visits `N(N-1)` directed contributions. Total force work is `(K+1)N(N-1)`; state updates and validation add linear passes. Storage is Theta(N), including the two acceleration buffers. No interaction matrix or trajectory history is stored. Numerical failures throw; the in-place evolution does not roll back a partially updated state.

## OpenMP memory access

Only the target loop is parallelized: `parallel for default(none) schedule(static)`. The output buffer is sized before the region. Bodies and parameters are shared read-only; each worker accumulates locally and writes only `accelerations[i]`. The inner source loop and its order are identical to the sequential solver. The region's implicit barrier completes all writes before validation or velocity updates consume them.

No global reduction, lock or atomic is needed. An optional diagnostic pointer, null by default, is written only by target iteration zero using `omp_get_num_threads()`. It is read after the barrier. For an empty system no region is entered and the observed count is zero. Runtime team limits are not overridden; tests fail with a diagnostic if a requested team size is not obtained.

## Input and output contracts

- Every public solver validates finite positions/velocities, finite positive masses, and finite strictly positive `G` and `epsilon` before entering the parallel region.
- Acceleration output size must equal the body count; callers allocate it first. Existing values are overwritten. A mismatch throws `std::invalid_argument` before writes.
- Simulation additionally validates the solver selection and finite strictly positive `dt`. Its squared-step coefficient must remain finite and nonzero.
- An empty system is valid with valid parameters, remains empty, and produces an empty acceleration array. Parameter validation still applies.
- One body has zero acceleration and moves at constant velocity. Zero steps validate/evaluate the initial state but leave positions and velocities unchanged.
- The CLI rejects malformed/out-of-range numbers, unknown options, negative step counts, nonpositive thread counts, and `--threads` in sequential mode. Errors produce a message and nonzero exit status.
- Non-finite force results and unsupported denominators are reported outside the OpenMP region. The checked simulation scans the evolving state between phases. The benchmark runs that checked simulation as its preflight. For timed repeats it checks input and final outputs outside the clock and retains the same per-pair guard.

Double precision and direct products have a limited range. This implementation is intended for moderate finite inputs; it does not guarantee accurate results for extreme scales, all underflow cases, or long unstable trajectories. Validation is deliberately kept in the baseline and is not a profiling optimization.

## Tests and observed results

Float comparisons for physics and solver equivalence reject non-finite values and use the predefined condition:

```text
abs(actual - expected) <= 1e-12 + 1e-12 * max(abs(actual), abs(expected))
```

The earlier Vec3 tests retain their absolute tolerance `1e-12` on small reference values. All checks use explicit failure counts/exit codes, including in Release.

Independent one-step reference (`G=1`, `epsilon=4`, `dt=0.5`):

| Quantity | Expected y component | Observed y component (Release, both solvers) |
| --- | --- | --- |
| Position A | 0.03 | 0.029999999999999999 |
| Position B | 2.994 | 2.9940000000000002 |
| Velocity A | 0.12005194016285471 | 0.12005194016285471 |
| Velocity B | -0.024010388032570942 | -0.024010388032570943 |

All x and z components above remain zero. Expected constants are literals independent of the implementation. The force balance `m_A*a_A + m_B*a_B = 0` passes. Additional checks cover vector operators, input preservation, overwritten outputs, isolated uniform motion, coincident softened bodies, empty systems, zero steps, rejected invalid inputs, numeric failure reporting, and acceleration reuse (10 continuous steps versus 10 restarted single steps).

The two examples are compared after 0, 1, 2 and 25 steps at `dt=0.01`, including initial/final accelerations, positions and velocities. In the force calculation, requested/observed thread counts were respectively **1/1, 2/2 and 4/4**. Independent physical references are tested for both solvers as well; agreement between two implementations alone is insufficient evidence of correct physics.

The six CTest entries (`openmp_smoke_check`, `vec3_tests`, `simulation_tests`, `demo_sequential`, `demo_openmp`, `cli_validation`) passed in **Debug: 6/6** and **Release: 6/6**, without compiler warnings. The CLI validation entry checks 14 rejected invocations, requiring both exit code 1 and the expected diagnostic. Both solver modes were also run directly on the two-body one-step and multi-body 25-step examples.

## Pending work

The benchmark checks both dynamic schedules against the same sequential reference and tolerance used for the static baseline. Confirm the individual input tuple and study convergence and longer-term invariants separately. [Scheduling comparison and reproduction](../README.md#does-dynamic-scheduling-reduce-total-time)
