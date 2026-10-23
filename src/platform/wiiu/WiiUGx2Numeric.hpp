#pragma once

#if HELENGINE_WIIU_HAS_GENERATED_CORE
#include "Real.hpp"
#include "RealEvaluation.hpp"
#include "RealMath.hpp"
#else
#include <cmath>
#endif

namespace helengine::wiiu {
#if HELENGINE_WIIU_HAS_GENERATED_CORE
    /// Preserves the configured authored storage representation in the CPU command queue.
    using WiiUGx2Real = ::Real;
    /// Preserves the configured arithmetic representation until the GX2 packing boundary.
    using WiiUGx2Evaluation = ::RealEvaluation;
    /// Dispatches rounding through the generated numeric policy.
    using WiiUGx2Math = ::RealMath;
#else
    /// Retains the existing float storage contract for standalone Core-less diagnostics.
    using WiiUGx2Real = float;
    /// Retains standalone float arithmetic without requiring generated headers.
    using WiiUGx2Evaluation = float;
    /// Supplies the rounding operations needed by standalone diagnostic scissor packing.
    class WiiUGx2Math {
    public:
        /// Rounds a standalone coordinate down to its pixel boundary.
        static float Floor(float value) { return std::floor(value); }
        /// Rounds a standalone coordinate up to its pixel boundary.
        static float Ceiling(float value) { return std::ceil(value); }
    };
#endif
}
