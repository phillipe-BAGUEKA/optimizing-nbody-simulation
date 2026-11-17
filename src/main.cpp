#include "nbody/examples.hpp"
#include "nbody/simulation.hpp"

#include <charconv>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

#include <omp.h>

namespace
{

struct Options
{
    nbody::AccelerationSolver solver{nbody::AccelerationSolver::Sequential};
    std::string_view case_name{"two-body"};
    std::size_t steps{10};
    double time_step{0.01};
    int threads{1};
    bool threads_specified{false};
    bool diagnostics{false};
    bool help{false};
};

Options parse_options(int argc, char* argv[])
{
    Options options;
    for (int index = 1; index < argc; ++index)
    {
        const std::string_view argument = argv[index];
        if (argument == "--help")
        {
            options.help = true;
            continue;
        }
        if (argument == "--diagnostics")
        {
            options.diagnostics = true;
            continue;
        }
        if (index + 1 >= argc)
        {
            throw std::invalid_argument("Missing value for " + std::string(argument));
        }
        const std::string_view value = argv[++index];
        if (argument == "--solver")
        {
            if (value == "sequential")
            {
                options.solver = nbody::AccelerationSolver::Sequential;
            }
            else if (value == "openmp")
            {
                options.solver = nbody::AccelerationSolver::OpenMP;
            }
            else
            {
                throw std::invalid_argument("Solver must be sequential or openmp.");
            }
        }
        else if (argument == "--case")
        {
            if (value != "two-body" && value != "multi-body")
            {
                throw std::invalid_argument("Case must be two-body or multi-body.");
            }
            options.case_name = value;
        }
        else if (argument == "--steps" || argument == "--dt" || argument == "--threads")
        {
            std::from_chars_result parsed{};
            const char* begin = value.data();
            const char* end = begin + value.size();
            if (argument == "--steps")
            {
                parsed = std::from_chars(begin, end, options.steps);
            }
            else if (argument == "--dt")
            {
                parsed = std::from_chars(begin, end, options.time_step);
            }
            else
            {
                parsed = std::from_chars(begin, end, options.threads);
                options.threads_specified = true;
            }
            if (parsed.ec != std::errc{} || parsed.ptr != end)
            {
                throw std::invalid_argument("Invalid numeric value for " + std::string(argument));
            }
        }
        else
        {
            throw std::invalid_argument("Unknown option: " + std::string(argument));
        }
    }
    if (!std::isfinite(options.time_step) || options.time_step <= 0.0)
    {
        throw std::invalid_argument("Time step must be finite and strictly positive.");
    }
    if (options.threads <= 0)
    {
        throw std::invalid_argument("Thread count must be strictly positive.");
    }
    if (options.threads_specified && options.solver != nbody::AccelerationSolver::OpenMP)
    {
        throw std::invalid_argument("--threads is only valid with --solver openmp.");
    }
    return options;
}

void print_state(std::string_view label, const std::vector<nbody::Body>& bodies)
{
    std::cout << label << " state (normalized units):\n";
    for (std::size_t index = 0; index < bodies.size(); ++index)
    {
        const nbody::Body& body = bodies[index];
        std::cout << "  body " << index << ": mass=" << body.mass
                  << " position=(" << body.position.x << ", " << body.position.y
                  << ", " << body.position.z << ") velocity=(" << body.velocity.x
                  << ", " << body.velocity.y << ", " << body.velocity.z << ")\n";
    }
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        const Options options = parse_options(argc, argv);
        if (options.help)
        {
            std::cout << "Usage: nbody_demo [--solver sequential|openmp]\n"
                      << "                  [--case two-body|multi-body] [--steps N] [--dt VALUE]\n"
                      << "                  [--threads N (OpenMP only)] [--diagnostics]\n"
                      << "Defaults: sequential, two-body, 10 steps, dt=0.01, 1 OpenMP thread.\n";
            return 0;
        }

        const bool parallel = options.solver == nbody::AccelerationSolver::OpenMP;
        if (parallel)
        {
            omp_set_dynamic(0);
            omp_set_num_threads(options.threads);
        }
        std::vector<nbody::Body> bodies = options.case_name == "two-body"
            ? nbody::make_two_body_example() : nbody::make_multi_body_example();
        const nbody::GravityParameters parameters{
            1.0, options.case_name == "two-body" ? 4.0 : 0.1
        };

        std::cout << std::setprecision(17)
                  << "Solver: " << (parallel ? "openmp" : "sequential")
                  << "\nCase: " << options.case_name
                  << "\nSteps: " << options.steps << "\nTime step: " << options.time_step
                  << "\nG: " << parameters.gravitational_constant
                  << "\nSoftening: " << parameters.softening << '\n';
        if (parallel)
        {
            std::cout << "Requested OpenMP threads: " << options.threads << '\n';
        }
        print_state("Initial", bodies);

        int observed_threads = 0;
        nbody::simulate_velocity_verlet(
            bodies, parameters, options.time_step, options.steps, options.solver,
            options.diagnostics ? &observed_threads : nullptr
        );

        if (options.diagnostics)
        {
            std::cout << "Observed solver threads (last evaluation): " << observed_threads << '\n';
        }
        print_state("Final", bodies);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
