#ifndef NBODY_GRAVITY_KERNEL_HPP
#define NBODY_GRAVITY_KERNEL_HPP

#include "nbody/gravity.hpp"

// Internal loops shared by the checked APIs and the prevalidated measurement.
// The caller owns the size and input checks. Both loops overwrite every target.
namespace nbody::detail
{
void fill_accelerations_sequential(
    const std::vector<Body>& bodies, const GravityParameters& parameters,
    std::vector<Vec3>& accelerations
);
void fill_accelerations_openmp(
    const std::vector<Body>& bodies, const GravityParameters& parameters,
    std::vector<Vec3>& accelerations, int* observed_thread_count
);
void validate_accelerations(const std::vector<Vec3>& accelerations);
} // namespace nbody::detail

#endif
