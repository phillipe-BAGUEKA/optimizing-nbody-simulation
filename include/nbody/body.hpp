#ifndef NBODY_BODY_HPP
#define NBODY_BODY_HPP

#include "nbody/vec3.hpp"

namespace nbody
{

struct Body
{
    double mass{1.0};
    Vec3 position{};
    Vec3 velocity{};
};

} // namespace nbody

#endif
