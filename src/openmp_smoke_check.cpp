#include <iostream>

#include <omp.h>

int main()
{
    constexpr long long element_count = 100'000;
    constexpr long long expected_sum =
        element_count * (element_count + 1) / 2;

    long long computed_sum = 0;
    int actual_thread_count = 0;

#pragma omp parallel reduction(+ : computed_sum) shared(actual_thread_count)
    {
#pragma omp single
        {
            actual_thread_count = omp_get_num_threads();
        }

#pragma omp for
        for (long long value = 1; value <= element_count; ++value)
        {
            computed_sum += value;
        }
    }

    const bool calculation_is_correct = computed_sum == expected_sum;

    std::cout << "OpenMP version: " << _OPENMP << '\n';
    std::cout << "Maximum available threads: " << omp_get_max_threads() << '\n';
    std::cout << "Threads used in parallel region: " << actual_thread_count << '\n';
    std::cout << "Computed sum: " << computed_sum << '\n';
    std::cout << "Expected sum: " << expected_sum << '\n';
    std::cout << "Check: " << (calculation_is_correct ? "PASS" : "FAIL") << '\n';

    return calculation_is_correct ? 0 : 1;
}
