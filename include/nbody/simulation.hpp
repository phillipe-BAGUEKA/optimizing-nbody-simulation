#ifndef NBODY_SIMULATION_HPP
#define NBODY_SIMULATION_HPP

#include "nbody/gravity.hpp"

#include <cstddef>
#include <vector>

namespace nbody
{

enum class AccelerationSolver
{
    Sequential,
    OpenMP
};

// In-place Velocity Verlet: steps + 1 acceleration evaluations, including steps == 0.
// Invalid inputs throw before state changes. A numerical failure during evolution
// throws without rolling back the partially updated state; discard that result.
void simulate_velocity_verlet(
    std::vector<Body>& bodies,
    const GravityParameters& parameters,
    double time_step,
    std::size_t steps,
    AccelerationSolver solver,
    int* observed_thread_count = nullptr
);

// Benchmark entry point for a configuration already checked against a reference.
// Returns elapsed seconds and leaves the final state in bodies. Input and final
// validation run outside the interval; the numerical loops and pair guards are shared.
double measure_velocity_verlet(
    std::vector<Body>& bodies, const GravityParameters& parameters,
    double time_step, std::size_t steps, AccelerationSolver solver
);

} // namespace nbody

#endif
