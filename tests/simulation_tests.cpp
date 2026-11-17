#include "nbody/examples.hpp"
#include "nbody/simulation.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include <omp.h>

namespace
{

int failure_count = 0;
constexpr double absolute_tolerance = 1.0e-12;
constexpr double relative_tolerance = 1.0e-12;

void expect(bool condition, const std::string& label)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << label << '\n';
        ++failure_count;
    }
}

void expect_near(double actual, double expected, const std::string& label)
{
    const double tolerance = absolute_tolerance
        + relative_tolerance * std::max(std::abs(actual), std::abs(expected));
    if (!std::isfinite(actual) || !std::isfinite(expected)
        || std::abs(actual - expected) > tolerance)
    {
        std::cerr << std::setprecision(17) << "FAIL: " << label
                  << ": expected " << expected << ", got " << actual << '\n';
        ++failure_count;
    }
}

void expect_vector(const nbody::Vec3& actual, const nbody::Vec3& expected, const std::string& label)
{
    expect_near(actual.x, expected.x, label + " x");
    expect_near(actual.y, expected.y, label + " y");
    expect_near(actual.z, expected.z, label + " z");
}

bool same_vector(const nbody::Vec3& left, const nbody::Vec3& right)
{
    return left.x == right.x && left.y == right.y && left.z == right.z;
}

void expect_unchanged(
    const std::vector<nbody::Body>& actual,
    const std::vector<nbody::Body>& original,
    const std::string& label
)
{
    expect(actual.size() == original.size(), label + " size");
    for (std::size_t index = 0; index < std::min(actual.size(), original.size()); ++index)
    {
        expect(actual[index].mass == original[index].mass, label + " mass");
        expect(same_vector(actual[index].position, original[index].position), label + " position");
        expect(same_vector(actual[index].velocity, original[index].velocity), label + " velocity");
    }
}

void expect_invalid(const std::function<void()>& operation, const std::string& label)
{
    try
    {
        operation();
        expect(false, label + " should throw invalid_argument");
    }
    catch (const std::invalid_argument&)
    {
    }
    catch (const std::exception& error)
    {
        expect(false, label + " unexpected exception: " + error.what());
    }
}

void compute(
    nbody::AccelerationSolver solver,
    const std::vector<nbody::Body>& bodies,
    const nbody::GravityParameters& parameters,
    std::vector<nbody::Vec3>& accelerations
)
{
    if (solver == nbody::AccelerationSolver::Sequential)
    {
        nbody::compute_accelerations_sequential(bodies, parameters, accelerations);
    }
    else
    {
        nbody::compute_accelerations_openmp(bodies, parameters, accelerations);
    }
}

void check_physical_reference(nbody::AccelerationSolver solver)
{
    const auto original = nbody::make_two_body_example();
    auto bodies = original;
    const nbody::GravityParameters parameters{1.0, 4.0};
    std::vector<nbody::Vec3> accelerations(bodies.size(), {99.0, 99.0, 99.0});
    compute(solver, bodies, parameters, accelerations);
    expect_vector(accelerations[0], {0.0, 0.24, 0.0}, "reference acceleration A");
    expect_vector(accelerations[1], {0.0, -0.048, 0.0}, "reference acceleration B");
    expect_unchanged(bodies, original, "gravity input preservation");
    expect_vector(
        accelerations[0] * bodies[0].mass + accelerations[1] * bodies[1].mass,
        {}, "opposite forces"
    );
    compute(solver, bodies, parameters, accelerations);
    expect_vector(accelerations[0], {0.0, 0.24, 0.0}, "output is overwritten");

    nbody::simulate_velocity_verlet(bodies, parameters, 0.5, 1, solver);
    // Independent arithmetic references supplied for this test; no solver-derived oracle.
    expect_vector(bodies[0].position, {0.0, 0.03, 0.0}, "Verlet position A");
    expect_vector(bodies[1].position, {0.0, 2.994, 0.0}, "Verlet position B");
    expect_vector(bodies[0].velocity, {0.0, 0.12005194016285471, 0.0}, "Verlet velocity A");
    expect_vector(bodies[1].velocity, {0.0, -0.024010388032570942, 0.0}, "Verlet velocity B");
    expect(bodies[0].mass == 2.0 && bodies[1].mass == 10.0, "masses preserved");
}

void check_edge_cases(nbody::AccelerationSolver solver)
{
    const nbody::GravityParameters parameters{1.0, 0.1};
    auto zero_steps = nbody::make_multi_body_example();
    const auto original = zero_steps;
    nbody::simulate_velocity_verlet(zero_steps, parameters, 0.01, 0, solver);
    expect_unchanged(zero_steps, original, "zero steps");

    std::vector<nbody::Body> empty;
    std::vector<nbody::Vec3> no_accelerations;
    compute(solver, empty, parameters, no_accelerations);
    int observed = -1;
    nbody::simulate_velocity_verlet(empty, parameters, 0.1, 3, solver, &observed);
    expect(empty.empty() && no_accelerations.empty(), "empty system stays empty");
    expect(observed == 0, "empty system launches no solver threads");

    std::vector<nbody::Body> isolated{{2.0, {1.0, -2.0, 3.0}, {0.5, -1.0, 2.0}}};
    std::vector<nbody::Vec3> acceleration(1);
    compute(solver, isolated, parameters, acceleration);
    expect_vector(acceleration[0], {}, "no self interaction");
    nbody::simulate_velocity_verlet(isolated, parameters, 0.25, 8, solver);
    expect_vector(isolated[0].position, {2.0, -4.0, 7.0}, "uniform isolated motion");
    expect_vector(isolated[0].velocity, {0.5, -1.0, 2.0}, "constant isolated velocity");

    std::vector<nbody::Body> coincident{
        {1.0, {1.0, 2.0, 3.0}, {}}, {2.0, {1.0, 2.0, 3.0}, {}}
    };
    std::vector<nbody::Vec3> coincident_accelerations(2);
    compute(solver, coincident, parameters, coincident_accelerations);
    expect_vector(coincident_accelerations[0], {}, "softened coincident target A");
    expect_vector(coincident_accelerations[1], {}, "softened coincident target B");

    auto continuous = nbody::make_multi_body_example();
    auto restarted = continuous;
    nbody::simulate_velocity_verlet(continuous, parameters, 0.01, 10, solver);
    for (int step = 0; step < 10; ++step)
    {
        // Restarting recomputes the acceleration; this detects stale cached values.
        nbody::simulate_velocity_verlet(restarted, parameters, 0.01, 1, solver);
    }
    for (std::size_t index = 0; index < continuous.size(); ++index)
    {
        expect_vector(continuous[index].position, restarted[index].position, "acceleration reuse position");
        expect_vector(continuous[index].velocity, restarted[index].velocity, "acceleration reuse velocity");
    }
}

void check_validation(nbody::AccelerationSolver solver)
{
    const auto original = nbody::make_two_body_example();
    const nbody::GravityParameters parameters{1.0, 4.0};
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (double invalid : {0.0, -1.0, infinity, nan})
    {
        auto bodies = original;
        bodies[0].mass = invalid;
        std::vector<nbody::Vec3> output(2, {7.0, 8.0, 9.0});
        expect_invalid([&] { compute(solver, bodies, parameters, output); }, "invalid mass");
        expect_vector(output[0], {7.0, 8.0, 9.0}, "validation before output writes");
        expect_invalid(
            [&] { compute(solver, original, {invalid, 4.0}, output); }, "invalid G"
        );
        expect_invalid(
            [&] { compute(solver, original, {1.0, invalid}, output); }, "invalid softening"
        );
        bodies = original;
        expect_invalid(
            [&] { nbody::simulate_velocity_verlet(bodies, parameters, invalid, 1, solver); },
            "invalid dt"
        );
        expect_unchanged(bodies, original, "invalid dt preserves input");
    }
    for (bool invalid_position : {false, true})
    {
        auto bodies = original;
        if (invalid_position)
        {
            bodies[1].position.z = nan;
        }
        else
        {
            bodies[1].velocity.y = infinity;
        }
        std::vector<nbody::Vec3> output(2);
        expect_invalid([&] { compute(solver, bodies, parameters, output); }, "non-finite state");
    }
    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{3}})
    {
        std::vector<nbody::Vec3> output(size);
        expect_invalid([&] { compute(solver, original, parameters, output); }, "output size");
    }
    std::vector<nbody::Body> empty;
    std::vector<nbody::Vec3> output;
    expect_invalid([&] { compute(solver, empty, {0.0, 4.0}, output); }, "validate empty system");

    auto bodies = original;
    expect_invalid(
        [&] { nbody::simulate_velocity_verlet(bodies, {1.0, 0.0}, 0.1, 0, solver); },
        "validate zero-step system"
    );
    expect_invalid(
        [&] { nbody::simulate_velocity_verlet(bodies, parameters, 1.0e300, 1, solver); },
        "dt square overflow"
    );
    expect_unchanged(bodies, original, "invalid simulation parameters preserve input");

    bodies[1].position.y = std::numeric_limits<double>::max();
    output.resize(2);
    try
    {
        compute(solver, bodies, parameters, output);
        expect(false, "unsupported gravitational arithmetic must fail");
    }
    catch (const std::overflow_error&)
    {
    }
}

void check_parallel_equivalence()
{
    for (int requested_threads : {1, 2, 4})
    {
        omp_set_num_threads(requested_threads);
        for (bool two_body : {true, false})
        {
            const auto initial = two_body
                ? nbody::make_two_body_example() : nbody::make_multi_body_example();
            const nbody::GravityParameters parameters{1.0, two_body ? 4.0 : 0.1};
            std::vector<nbody::Vec3> sequential(initial.size());
            std::vector<nbody::Vec3> parallel(initial.size());
            nbody::compute_accelerations_sequential(initial, parameters, sequential);
            int observed_threads = 0;
            nbody::compute_accelerations_openmp(initial, parameters, parallel, &observed_threads);
            std::cout << "Case=" << (two_body ? "two-body" : "multi-body")
                      << " requested_threads=" << requested_threads
                      << " observed_threads=" << observed_threads << '\n';
            expect(observed_threads == requested_threads, "actual OpenMP team size");
            for (std::size_t index = 0; index < initial.size(); ++index)
            {
                expect_vector(parallel[index], sequential[index], "initial solver equivalence");
            }
            for (std::size_t steps : {std::size_t{0}, std::size_t{1}, std::size_t{2}, std::size_t{25}})
            {
                auto reference = initial;
                auto candidate = initial;
                nbody::simulate_velocity_verlet(
                    reference, parameters, 0.01, steps, nbody::AccelerationSolver::Sequential
                );
                nbody::simulate_velocity_verlet(
                    candidate, parameters, 0.01, steps, nbody::AccelerationSolver::OpenMP,
                    &observed_threads
                );
                expect(observed_threads == requested_threads, "simulation team size");
                nbody::compute_accelerations_sequential(reference, parameters, sequential);
                nbody::compute_accelerations_openmp(candidate, parameters, parallel);
                for (std::size_t index = 0; index < initial.size(); ++index)
                {
                    expect_vector(candidate[index].position, reference[index].position,
                                  "position equivalence at step " + std::to_string(steps));
                    expect_vector(candidate[index].velocity, reference[index].velocity,
                                  "velocity equivalence at step " + std::to_string(steps));
                    expect_vector(parallel[index], sequential[index], "final acceleration equivalence");
                }
            }
        }
    }
}

void check_arrival_trace()
{
    omp_set_num_threads(4);
    std::vector<nbody::Body> empty;
    const nbody::GravityParameters parameters{1.0, 4.0};
    const auto empty_trace = nbody::trace_openmp_arrivals(empty, parameters, 0.5, 0);
    expect(empty_trace.size() == 1 && empty_trace.front().empty(), "empty system launches no workers");

    auto bodies = nbody::make_two_body_example();
    auto reference = bodies;
    nbody::simulate_velocity_verlet(reference, parameters, 0.5, 1, nbody::AccelerationSolver::Sequential);
    const auto trace = nbody::trace_openmp_arrivals(bodies, parameters, 0.5, 1);
    expect(trace.size() == 2, "trace contains initial and updated force");
    for (const auto& evaluation : trace)
    {
        expect(evaluation.size() == 4, "trace includes all team members");
        std::vector<std::size_t> counts;
        for (const auto& arrival : evaluation)
        {
            counts.push_back(arrival.targets);
            expect(arrival.finish_offset.count() >= 0, "finish follows the evaluation origin");
        }
        std::sort(counts.begin(), counts.end());
        expect(counts == std::vector<std::size_t>{0, 0, 1, 1}, "idle workers also record a finish");
    }
    for (std::size_t i = 0; i < bodies.size(); ++i)
    {
        expect_vector(bodies[i].position, reference[i].position, "traced position");
        expect_vector(bodies[i].velocity, reference[i].velocity, "traced velocity");
    }
}

} // namespace

int main()
{
    omp_set_dynamic(0);
    omp_set_num_threads(2);
    try
    {
        for (const auto solver : {nbody::AccelerationSolver::Sequential, nbody::AccelerationSolver::OpenMP})
        {
            check_physical_reference(solver);
            check_edge_cases(solver);
            check_validation(solver);
        }
        check_parallel_equivalence();
        check_arrival_trace();
    }
    catch (const std::exception& error)
    {
        expect(false, std::string("Unexpected exception: ") + error.what());
    }
    if (failure_count != 0)
    {
        std::cerr << failure_count << " simulation check(s) failed.\n";
        return 1;
    }
    std::cout << "All physical, integration, validation and OpenMP checks passed.\n";
    return 0;
}
