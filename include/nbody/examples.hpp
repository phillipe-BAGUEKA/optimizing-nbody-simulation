#ifndef NBODY_EXAMPLES_HPP
#define NBODY_EXAMPLES_HPP

#include "nbody/body.hpp"

#include <vector>

namespace nbody
{

// Provisional fixtures in normalized units, documented in docs/model-and-validation.md.
inline std::vector<Body> make_two_body_example()
{
    return {
        {2.0, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}},
        {10.0, {0.0, 3.0, 0.0}, {0.0, 0.0, 0.0}}
    };
}

inline std::vector<Body> make_multi_body_example()
{
    return {
        {2.0, {-1.0, 0.0, 0.5}, {0.05, 0.10, -0.02}},
        {1.5, {1.0, 0.5, -0.5}, {-0.10, 0.02, 0.03}},
        {0.75, {0.0, -1.0, 1.0}, {0.04, -0.03, 0.08}},
        {3.0, {0.5, 1.5, 0.25}, {-0.02, 0.06, -0.04}}
    };
}

} // namespace nbody

#endif
