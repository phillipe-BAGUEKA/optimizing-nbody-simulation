#include "nbody/vec3.hpp"

#include <cmath>
#include <iostream>
#include <string_view>

namespace
{

constexpr double comparison_tolerance = 1.0e-12;

void expect_near(
    std::string_view case_name,
    std::string_view value_name,
    double actual,
    double expected,
    int& failure_count
)
{
    const bool value_is_valid =
        std::isfinite(actual)
        && std::abs(actual - expected) <= comparison_tolerance;

    if (!value_is_valid)
    {
        std::cerr << "FAIL: " << case_name << " - " << value_name
                  << ": expected " << expected << ", got " << actual << '\n';
        ++failure_count;
    }
}

void check_vec3_case(
    std::string_view case_name,
    const nbody::Vec3& vector,
    double expected_squared_norm,
    double expected_norm,
    int& failure_count
)
{
    const double original_x = vector.x;
    const double original_y = vector.y;
    const double original_z = vector.z;

    expect_near(
        case_name,
        "squared norm",
        vector.squared_norm(),
        expected_squared_norm,
        failure_count
    );
    expect_near(
        case_name,
        "norm",
        vector.norm(),
        expected_norm,
        failure_count
    );
    expect_near(case_name, "preserved x", vector.x, original_x, failure_count);
    expect_near(case_name, "preserved y", vector.y, original_y, failure_count);
    expect_near(case_name, "preserved z", vector.z, original_z, failure_count);
}

} // namespace

int main()
{
    int failure_count = 0;

    const nbody::Vec3 default_vector{};
    expect_near(
        "default initialization",
        "x",
        default_vector.x,
        0.0,
        failure_count
    );
    expect_near(
        "default initialization",
        "y",
        default_vector.y,
        0.0,
        failure_count
    );
    expect_near(
        "default initialization",
        "z",
        default_vector.z,
        0.0,
        failure_count
    );

    const nbody::Vec3 three_four_zero{3.0, 4.0, 0.0};
    check_vec3_case("(3, 4, 0)", three_four_zero, 25.0, 5.0, failure_count);

    const nbody::Vec3 signed_vector{-3.0, 4.0, -12.0};
    check_vec3_case("(-3, 4, -12)", signed_vector, 169.0, 13.0, failure_count);

    const nbody::Vec3 zero_vector{0.0, 0.0, 0.0};
    check_vec3_case("(0, 0, 0)", zero_vector, 0.0, 0.0, failure_count);

    const nbody::Vec3 decimal_vector{1.5, -2.0, 6.0};
    check_vec3_case("(1.5, -2, 6)", decimal_vector, 42.25, 6.5, failure_count);

    const nbody::Vec3 left{1.0, -2.0, 3.0};
    const nbody::Vec3 right{-4.0, 5.0, 0.5};
    const nbody::Vec3 sum = left + right;
    expect_near("addition", "x", sum.x, -3.0, failure_count);
    expect_near("addition", "y", sum.y, 3.0, failure_count);
    expect_near("addition", "z", sum.z, 3.5, failure_count);
    const nbody::Vec3 difference = left - right;
    expect_near("subtraction", "x", difference.x, 5.0, failure_count);
    expect_near("subtraction", "y", difference.y, -7.0, failure_count);
    expect_near("subtraction", "z", difference.z, 2.5, failure_count);
    const nbody::Vec3 scaled = left * -2.0;
    expect_near("scaling", "x", scaled.x, -2.0, failure_count);
    expect_near("scaling", "y", scaled.y, 4.0, failure_count);
    expect_near("scaling", "z", scaled.z, -6.0, failure_count);
    nbody::Vec3 accumulated = left;
    nbody::Vec3& returned = (accumulated += right);
    expect_near("accumulation", "x", accumulated.x, -3.0, failure_count);
    expect_near("accumulation", "y", accumulated.y, 3.0, failure_count);
    expect_near("accumulation", "z", accumulated.z, 3.5, failure_count);
    if (&returned != &accumulated)
    {
        std::cerr << "FAIL: operator+= must return a reference to its left operand.\n";
        ++failure_count;
    }
    accumulated += accumulated;
    expect_near("self accumulation", "x", accumulated.x, -6.0, failure_count);
    expect_near("self accumulation", "y", accumulated.y, 6.0, failure_count);
    expect_near("self accumulation", "z", accumulated.z, 7.0, failure_count);

    if (failure_count != 0)
    {
        std::cerr << failure_count << " Vec3 check(s) failed.\n";
        return 1;
    }

    std::cout << "All Vec3 checks passed.\n";
    return 0;
}
