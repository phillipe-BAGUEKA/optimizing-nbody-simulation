#include "nbody/simulation.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <omp.h>

namespace
{
constexpr nbody::GravityParameters gravity{1.0, 0.05};
constexpr double dt = 0.0005;

template<typename Integer>
Integer read_integer(std::string_view text, const char* name)
{
    Integer value{};
    const char* end = text.data() + text.size();
    const auto result = std::from_chars(text.data(), end, value);
    if (result.ec != std::errc{} || result.ptr != end)
    {
        throw std::invalid_argument(std::string("Invalid ") + name);
    }
    return value;
}

int read_threads(std::string_view argument)
{
    const int count = read_integer<int>(argument, "thread count");
    if (count < 1 || count > std::min(omp_get_num_procs(), omp_get_thread_limit()))
    {
        throw std::invalid_argument("Requested thread count is outside the available range.");
    }
    return count;
}

std::vector<nbody::Body> make_initial_state(std::size_t count, std::uint64_t seed)
{
    std::mt19937_64 generator(seed);
    const auto unit = [&generator]()
    {
        // Use the high 53 bits so the conversion is independent of distribution libraries.
        return static_cast<double>(generator() >> 11) * 0x1.0p-53;
    };
    const auto component = [&unit]() { return 2.0 * unit() - 1.0; };
    std::vector<nbody::Body> bodies(count);
    for (auto& body : bodies)
    {
        body.mass = 0.5 + unit();
        body.position = {component(), component(), component()};
        body.velocity = {0.1 * component(), 0.1 * component(), 0.1 * component()};
    }
    return bodies;
}

std::uint64_t state_fingerprint(const std::vector<nbody::Body>& bodies)
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& body : bodies)
    {
        for (double value : {body.mass, body.position.x, body.position.y, body.position.z,
                             body.velocity.x, body.velocity.y, body.velocity.z})
        {
            auto bits = std::bit_cast<std::uint64_t>(value);
            for (int byte = 0; byte < 8; ++byte)
            {
                hash ^= bits & 0xff;
                hash *= 1099511628211ULL;
                bits >>= 8;
            }
        }
    }
    return hash;
}

bool matches(double actual, double expected)
{
    return std::isfinite(actual) && std::isfinite(expected)
        && std::abs(actual - expected) <= 1e-12 + 1e-12 * std::max(std::abs(actual), std::abs(expected));
}

bool matches(const nbody::Vec3& actual, const nbody::Vec3& expected)
{
    return matches(actual.x, expected.x) && matches(actual.y, expected.y) && matches(actual.z, expected.z);
}

void compare_final_state(const std::vector<nbody::Body>& state, const std::vector<nbody::Body>& reference)
{
    if (state.size() != reference.size())
    {
        throw std::runtime_error("Body count differs from the sequential reference.");
    }
    for (std::size_t i = 0; i < state.size(); ++i)
    {
        if (state[i].mass != reference[i].mass || !matches(state[i].position, reference[i].position)
            || !matches(state[i].velocity, reference[i].velocity))
        {
            throw std::runtime_error("Final state differs from the sequential reference at body "
                                     + std::to_string(i));
        }
    }
}

void write_state(const std::string& path, const std::vector<nbody::Body>& state)
{
    std::ofstream output(path);
    output.exceptions(std::ios::badbit | std::ios::failbit);
    output << std::setprecision(17) << "index,mass,x,y,z,vx,vy,vz\n";
    for (std::size_t i = 0; i < state.size(); ++i)
    {
        const auto& b = state[i];
        output << i << ',' << b.mass << ',' << b.position.x << ',' << b.position.y << ','
               << b.position.z << ',' << b.velocity.x << ',' << b.velocity.y << ',' << b.velocity.z << '\n';
    }
    output.close();
}

struct Configuration
{
    nbody::AccelerationSolver solver;
    int requested;
    int observed{};
};

void campaign(const std::vector<nbody::Body>& initial, std::size_t steps, std::uint64_t seed,
              int repetitions, const std::string& state_path, std::vector<Configuration>& configurations)
{
    auto reference = initial;
    nbody::simulate_velocity_verlet(reference, gravity, dt, steps, nbody::AccelerationSolver::Sequential);
    const auto initial_hash = state_fingerprint(initial);
    for (auto& config : configurations)
    {
        omp_set_num_threads(config.requested);
        auto warmup = initial;
        nbody::simulate_velocity_verlet(warmup, gravity, dt, steps, config.solver, &config.observed);
        compare_final_state(warmup, reference);
        if (config.observed != config.requested)
        {
            throw std::runtime_error("Control requested " + std::to_string(config.requested)
                                     + " threads and observed " + std::to_string(config.observed));
        }
    }

    std::cout << "n,k,seed,g,epsilon,dt,round,order,solver,requested_threads,observed_control_threads,"
                 "elapsed_s,initial_hash,final_hash\n";
    for (int round = 0; round < repetitions; ++round)
    {
        for (std::size_t order = 0; order < configurations.size(); ++order)
        {
            const std::size_t index = (static_cast<std::size_t>(round) + order) % configurations.size();
            const auto& config = configurations[index];
            omp_set_num_threads(config.requested);
            auto state = initial;
            const double seconds = nbody::measure_velocity_verlet(state, gravity, dt, steps, config.solver);
            compare_final_state(state, reference);
            std::cout << initial.size() << ',' << steps << ',' << seed << ',' << gravity.gravitational_constant
                      << ',' << gravity.softening << ',' << dt << ',' << round + 1 << ',' << order + 1 << ','
                      << (config.solver == nbody::AccelerationSolver::Sequential ? "sequential" : "openmp")
                      << ',' << config.requested << ',' << config.observed << ',' << seconds << ','
                      << std::hex << initial_hash << ',' << state_fingerprint(state) << std::dec << '\n';
            if (state_path != "-" && round == repetitions - 1 && order == configurations.size() - 1)
            {
                write_state(state_path, state);
            }
        }
    }
}

void profile(const std::vector<nbody::Body>& initial, std::size_t steps, int repeats, int threads)
{
    // Use the already validated campaign case. A capture must not sample a sequential reference run.
    omp_set_num_threads(threads);
    for (int repeat = 0; repeat < repeats; ++repeat)
    {
        auto state = initial;
        nbody::measure_velocity_verlet(state, gravity, dt, steps, nbody::AccelerationSolver::OpenMP);
        std::cout << "profile_batch=" << repeat + 1 << " final_hash=" << std::hex
                  << state_fingerprint(state) << std::dec << '\n';
    }
}

} // namespace

int main(int argc, char* argv[])
{
    try
    {
        if (argc < 7)
        {
            throw std::invalid_argument(
                "Usage: nbody_benchmark campaign N K SEED R FINAL.csv|- [THREADS ...]\n"
                "       nbody_benchmark profile N K SEED R THREADS"
            );
        }
        const std::string_view mode = argv[1];
        const auto count = read_integer<std::size_t>(argv[2], "body count");
        const auto steps = read_integer<std::size_t>(argv[3], "step count");
        const auto seed = read_integer<std::uint64_t>(argv[4], "seed");
        const int repeats = read_integer<int>(argv[5], "repetition count");
        if (count == 0 || repeats < 1)
        {
            throw std::invalid_argument("Benchmarks require at least one body and one repetition.");
        }
        omp_set_dynamic(0);
        std::cout << std::setprecision(17);
        if (mode == "campaign")
        {
            std::vector<Configuration> configurations{{nbody::AccelerationSolver::Sequential, 1}};
            for (int index = 7; index < argc; ++index)
            {
                const int threads = read_threads(argv[index]);
                const auto duplicate = [threads](const Configuration& config)
                {
                    return config.solver == nbody::AccelerationSolver::OpenMP && config.requested == threads;
                };
                if (std::any_of(configurations.begin(), configurations.end(), duplicate))
                {
                    throw std::invalid_argument("Duplicate OpenMP configuration.");
                }
                configurations.push_back({nbody::AccelerationSolver::OpenMP, threads});
            }
            campaign(make_initial_state(count, seed), steps, seed, repeats, argv[6], configurations);
        }
        else if (mode == "profile" && argc == 7)
        {
            profile(make_initial_state(count, seed), steps, repeats, read_threads(argv[6]));
        }
        else
        {
            throw std::invalid_argument("Unknown mode or incorrect number of profile arguments.");
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
