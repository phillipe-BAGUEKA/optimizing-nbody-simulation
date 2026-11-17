#ifndef NBODY_GRAVITY_HPP
#define NBODY_GRAVITY_HPP

#include "nbody/body.hpp"

#include <chrono>
#include <cstddef>
#include <vector>

namespace nbody
{

struct GravityParameters
{
    double gravitational_constant{1.0};
    double softening{0.1};
};

// One worker's completed targets and time from the common start of a force evaluation.
struct ThreadArrival
{
    std::size_t targets{};
    std::chrono::nanoseconds finish_offset{};
};

// Reject invalid inputs before computation, including for empty systems.
void validate_gravity_inputs(
    const std::vector<Body>& bodies,
    const GravityParameters& parameters
);

// Output must already have bodies.size() elements; it is overwritten, not added to.
void compute_accelerations_sequential(
    const std::vector<Body>& bodies,
    const GravityParameters& parameters,
    std::vector<Vec3>& accelerations
);

// Optional diagnostic: actual team size, or zero for an empty system (no region).
// Input bodies are read-only; the function returns after the implicit barrier.
void compute_accelerations_openmp(
    const std::vector<Body>& bodies,
    const GravityParameters& parameters,
    std::vector<Vec3>& accelerations,
    int* observed_thread_count = nullptr
);

} // namespace nbody

#endif
