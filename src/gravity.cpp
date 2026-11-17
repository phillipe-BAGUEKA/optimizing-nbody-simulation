#include "nbody/gravity.hpp"
#include "gravity_kernel.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include <omp.h>

namespace nbody
{
namespace
{

bool is_finite(const Vec3& value)
{
    return std::isfinite(value.x)
        && std::isfinite(value.y)
        && std::isfinite(value.z);
}

Vec3 gravitational_contribution(
    const Body& target,
    const Body& source,
    const GravityParameters& parameters
)
{
    const Vec3 displacement = source.position - target.position;
    const double q = displacement.squared_norm()
        + parameters.softening * parameters.softening;
    const double denominator = q * std::sqrt(q);
    const double numerator = parameters.gravitational_constant * source.mass;

    if (!std::isfinite(denominator) || denominator <= 0.0
        || !std::isfinite(numerator))
    {
        // Never throw through an OpenMP region. The caller checks outputs afterwards.
        const double invalid = std::numeric_limits<double>::quiet_NaN();
        return {invalid, invalid, invalid};
    }

    return displacement * (numerator / denominator);
}

Vec3 acceleration_for_target(
    std::size_t target_index,
    const std::vector<Body>& bodies,
    const GravityParameters& parameters
)
{
    Vec3 acceleration{};
    for (std::size_t source_index = 0; source_index < bodies.size(); ++source_index)
    {
        if (source_index != target_index)
        {
            acceleration += gravitational_contribution(
                bodies[target_index], bodies[source_index], parameters
            );
        }
    }
    return acceleration;
}

void validate_output_size(
    const std::vector<Body>& bodies,
    const std::vector<Vec3>& accelerations
)
{
    if (accelerations.size() != bodies.size())
    {
        throw std::invalid_argument("Acceleration count must match body count.");
    }
}

} // namespace

void detail::validate_accelerations(const std::vector<Vec3>& accelerations)
{
    for (const Vec3& acceleration : accelerations)
    {
        if (!is_finite(acceleration))
        {
            throw std::overflow_error("Gravity calculation exceeded its numeric range.");
        }
    }
}

void validate_gravity_inputs(
    const std::vector<Body>& bodies,
    const GravityParameters& parameters
)
{
    if (!std::isfinite(parameters.gravitational_constant)
        || parameters.gravitational_constant <= 0.0)
    {
        throw std::invalid_argument("G must be finite and strictly positive.");
    }
    if (!std::isfinite(parameters.softening) || parameters.softening <= 0.0)
    {
        throw std::invalid_argument("Softening must be finite and strictly positive.");
    }
    for (const Body& body : bodies)
    {
        if (!std::isfinite(body.mass) || body.mass <= 0.0)
        {
            throw std::invalid_argument("Every mass must be finite and strictly positive.");
        }
        if (!is_finite(body.position) || !is_finite(body.velocity))
        {
            throw std::invalid_argument("Positions and velocities must be finite.");
        }
    }
}

void detail::fill_accelerations_sequential(
    const std::vector<Body>& bodies,
    const GravityParameters& parameters,
    std::vector<Vec3>& accelerations
)
{
    for (std::size_t target_index = 0; target_index < bodies.size(); ++target_index)
    {
        accelerations[target_index] = acceleration_for_target(target_index, bodies, parameters);
    }
}

void detail::fill_accelerations_openmp(
    const std::vector<Body>& bodies,
    const GravityParameters& parameters,
    std::vector<Vec3>& accelerations,
    int* observed_thread_count,
    int dynamic_chunk
)
{
    if (observed_thread_count != nullptr)
    {
        *observed_thread_count = 0;
    }
    const std::size_t body_count = bodies.size();
    if (body_count == 0)
    {
        return;
    }

    const auto evaluate = [&](std::size_t target_index)
    {
        if (observed_thread_count != nullptr && target_index == 0)
        {
            // Exactly one iteration writes the diagnostic; it is read after the barrier.
            *observed_thread_count = omp_get_num_threads();
        }
        accelerations[target_index] = acceleration_for_target(target_index, bodies, parameters);
    };
    if (dynamic_chunk > 0)
    {
#pragma omp parallel for default(none) schedule(dynamic, dynamic_chunk) \
    shared(evaluate) firstprivate(body_count, dynamic_chunk)
        for (std::size_t index = 0; index < body_count; ++index)
        {
            evaluate(index);
        }
    }
    else
    {
#pragma omp parallel for default(none) schedule(static) shared(evaluate) firstprivate(body_count)
        for (std::size_t index = 0; index < body_count; ++index)
        {
            evaluate(index);
        }
    }
}

std::vector<ThreadArrival> detail::trace_accelerations_openmp(
    const std::vector<Body>& bodies, const GravityParameters& parameters,
    std::vector<Vec3>& accelerations, int dynamic_chunk
)
{
    const std::size_t body_count = bodies.size();
    if (body_count == 0)
    {
        return {};
    }
    std::vector<ThreadArrival> arrivals(static_cast<std::size_t>(omp_get_max_threads()));
    int actual_threads = 0;
    using Clock = std::chrono::steady_clock;
    const auto origin = Clock::now();

#pragma omp parallel default(none) shared(bodies, parameters, accelerations, arrivals, actual_threads) \
    firstprivate(body_count, origin, dynamic_chunk)
    {
        const int thread_id = omp_get_thread_num();
        std::size_t targets = 0;
        const auto evaluate = [&](std::size_t index)
        {
            accelerations[index] = acceleration_for_target(index, bodies, parameters);
            ++targets;
        };
        if (dynamic_chunk > 0)
        {
#pragma omp for schedule(dynamic, dynamic_chunk) nowait
            for (std::size_t index = 0; index < body_count; ++index)
            {
                evaluate(index);
            }
        }
        else
        {
#pragma omp for schedule(static) nowait
            for (std::size_t index = 0; index < body_count; ++index)
            {
                evaluate(index);
            }
        }
        // Each worker timestamps its own loop exit before the parallel region joins.
        const auto finished = Clock::now();
        arrivals[static_cast<std::size_t>(thread_id)] = {
            targets, std::chrono::duration_cast<std::chrono::nanoseconds>(finished - origin)
        };
        if (thread_id == 0)
        {
            actual_threads = omp_get_num_threads();
        }
    }
    arrivals.resize(static_cast<std::size_t>(actual_threads));
    return arrivals;
}

void compute_accelerations_sequential(
    const std::vector<Body>& bodies, const GravityParameters& parameters,
    std::vector<Vec3>& accelerations
)
{
    validate_gravity_inputs(bodies, parameters);
    validate_output_size(bodies, accelerations);
    detail::fill_accelerations_sequential(bodies, parameters, accelerations);
    detail::validate_accelerations(accelerations);
}

void compute_accelerations_openmp(
    const std::vector<Body>& bodies, const GravityParameters& parameters,
    std::vector<Vec3>& accelerations, int* observed_thread_count
)
{
    validate_gravity_inputs(bodies, parameters);
    validate_output_size(bodies, accelerations);
    detail::fill_accelerations_openmp(bodies, parameters, accelerations, observed_thread_count);
    detail::validate_accelerations(accelerations);
}

} // namespace nbody
