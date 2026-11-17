#include "nbody/simulation.hpp"
#include "gravity_kernel.hpp"

#include <chrono>
#include <cmath>
#include <stdexcept>

namespace nbody
{
namespace
{

void run_velocity_verlet(
    std::vector<Body>& bodies,
    const GravityParameters& parameters,
    double time_step,
    std::size_t steps,
    AccelerationSolver solver,
    int* observed_thread_count,
    double* elapsed_seconds
)
{
    validate_gravity_inputs(bodies, parameters);
    if (!std::isfinite(time_step) || time_step <= 0.0)
    {
        throw std::invalid_argument("Time step must be finite and strictly positive.");
    }
    const double half_time_step_squared = 0.5 * time_step * time_step;
    if (!std::isfinite(half_time_step_squared) || half_time_step_squared == 0.0)
    {
        throw std::invalid_argument("Time step squared is outside the supported numeric range.");
    }
    if (solver != AccelerationSolver::Sequential && solver != AccelerationSolver::OpenMP)
    {
        throw std::invalid_argument("Unknown acceleration solver.");
    }

    const auto evaluate_accelerations = [&](std::vector<Vec3>& output)
    {
        if (elapsed_seconds == nullptr)
        {
            validate_gravity_inputs(bodies, parameters);
        }
        if (solver == AccelerationSolver::Sequential)
        {
            detail::fill_accelerations_sequential(bodies, parameters, output);
            if (observed_thread_count != nullptr)
            {
                *observed_thread_count = bodies.empty() ? 0 : 1;
            }
        }
        else
        {
            detail::fill_accelerations_openmp(bodies, parameters, output, observed_thread_count);
        }
        if (elapsed_seconds == nullptr)
        {
            detail::validate_accelerations(output);
        }
    };

    std::vector<Vec3> accelerations(bodies.size());
    std::vector<Vec3> new_accelerations(bodies.size());
    using Clock = std::chrono::steady_clock;
    const auto start = elapsed_seconds != nullptr ? Clock::now() : Clock::time_point{};
    evaluate_accelerations(accelerations);

    for (std::size_t step = 0; step < steps; ++step)
    {
        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            bodies[index].position += bodies[index].velocity * time_step
                + accelerations[index] * half_time_step_squared;
        }

        // Every position now belongs to the new time level.
        evaluate_accelerations(new_accelerations);

        for (std::size_t index = 0; index < bodies.size(); ++index)
        {
            bodies[index].velocity += (accelerations[index] + new_accelerations[index])
                * (0.5 * time_step);
        }
        if (elapsed_seconds == nullptr)
        {
            validate_gravity_inputs(bodies, parameters);
        }

        // Reuse the new accelerations without a third evaluation or a vector copy.
        accelerations.swap(new_accelerations);
    }
    if (elapsed_seconds != nullptr)
    {
        const auto end = Clock::now();
        *elapsed_seconds = std::chrono::duration<double>(end - start).count();
        detail::validate_accelerations(accelerations);
        validate_gravity_inputs(bodies, parameters);
    }
}

} // namespace

void simulate_velocity_verlet(
    std::vector<Body>& bodies, const GravityParameters& parameters,
    double time_step, std::size_t steps, AccelerationSolver solver, int* observed_thread_count
)
{
    run_velocity_verlet(bodies, parameters, time_step, steps, solver, observed_thread_count, nullptr);
}

double measure_velocity_verlet(
    std::vector<Body>& bodies, const GravityParameters& parameters,
    double time_step, std::size_t steps, AccelerationSolver solver
)
{
    double elapsed_seconds = 0.0;
    run_velocity_verlet(bodies, parameters, time_step, steps, solver, nullptr, &elapsed_seconds);
    return elapsed_seconds;
}

} // namespace nbody
