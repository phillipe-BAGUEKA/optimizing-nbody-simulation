#ifndef NBODY_VEC3_HPP
#define NBODY_VEC3_HPP

#include <cmath>

namespace nbody
{

struct Vec3
{
    double x{};
    double y{};
    double z{};

    Vec3 operator+(const Vec3& other) const
    {
        return {x + other.x, y + other.y, z + other.z};
    }

    Vec3 operator-(const Vec3& other) const
    {
        return {x - other.x, y - other.y, z - other.z};
    }

    Vec3 operator*(double scalar) const
    {
        return {x * scalar, y * scalar, z * scalar};
    }

    Vec3& operator+=(const Vec3& other)
    {
        x += other.x;
        y += other.y;
        z += other.z;
        return *this;
    }

    double squared_norm() const
    {
        return x * x + y * y + z * z;
    }

    double norm() const
    {
        return std::sqrt(squared_norm());
    }
};

} // namespace nbody

#endif
