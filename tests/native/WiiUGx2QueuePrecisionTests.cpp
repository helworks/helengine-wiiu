#if HELENGINE_WIIU_HAS_GENERATED_CORE
#include "helcpp_config.hpp"
// Select the numeric contract for this test translation unit, leaving generated files intact.
#undef HE_CPP_REAL_STORAGE_BITS
#undef HE_CPP_REAL_ARITHMETIC_BITS
#define HE_CPP_REAL_STORAGE_BITS HELENGINE_WIIU_TEST_STORAGE_BITS
#define HE_CPP_REAL_ARITHMETIC_BITS HELENGINE_WIIU_TEST_ARITHMETIC_BITS
#include "Real.hpp"
#endif

#include "platform/wiiu/WiiUGx2QuadGeometry.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <type_traits>

/// Checks queue storage and arithmetic before the final GX2 vertex/scissor packing boundary.
int main() {
    using namespace helengine::wiiu;
#if HELENGINE_WIIU_HAS_GENERATED_CORE
    static_assert(std::is_same_v<decltype(WiiUGx2QuadCommand::X), Real>);
    static_assert(std::is_same_v<decltype(WiiUGx2ClipRect::Width), Real>);
    static_assert(std::is_same_v<decltype(WiiUGx2QuadCommand::CornerX)::value_type, Real>);
#else
    static_assert(std::is_same_v<decltype(WiiUGx2QuadCommand::X), float>);
#endif
    WiiUGx2QuadCommand command {};
    command.X = 10;
    command.Y = 20;
    command.Width = 30;
    command.Height = 40;
    command.SourceX = 0.25f;
    command.SourceY = 0.5f;
    command.SourceWidth = 0.5f;
    command.SourceHeight = 0.25f;
    command.ClipRect = {10.25f, 20.25f, 30.5f, 40.5f};
    WiiUGx2RenderFrame frame;
    frame.AddQuad(command);
    float positions[12];
    float textureCoordinates[12];
    std::uint32_t scissor[4];
    WiiUGx2QuadGeometry::WritePositions(frame.GetQuadCommands()[0], 200, 100, positions);
    WiiUGx2QuadGeometry::WriteTextureCoordinates(command, textureCoordinates);
    WiiUGx2QuadGeometry::WriteScissor(command.ClipRect, 200, 100, 400, 200, scissor);
    if (std::abs(positions[0] + 0.9f) > 1e-6f || textureCoordinates[0] != 0.25f
        || textureCoordinates[1] != 0.75f || scissor[0] != 20 || scissor[1] != 40
        || scissor[2] != 62 || scissor[3] != 82) {
        throw std::runtime_error("GX2 queue packing changed logical rectangle, UV or scissor behavior");
    }
#if HELENGINE_WIIU_HAS_GENERATED_CORE && HELENGINE_WIIU_TEST_STORAGE_BITS == 64 && HELENGINE_WIIU_TEST_ARITHMETIC_BITS == 64
    command.X = Real(16777217.0);
    command.Width = Real(1.0);
    command.SourceX = Real(-16777216.0);
    command.SourceWidth = Real(16777217.0);
    frame.Clear();
    frame.AddQuad(command);
    if (frame.GetQuadCommands()[0].X.ToDouble() != 16777217.0) {
        throw std::runtime_error("GX2 CPU queue rounded configured 64-bit storage to float");
    }
    WiiUGx2QuadGeometry::WritePositions(frame.GetQuadCommands()[0], 16777216, 100, positions);
    WiiUGx2QuadGeometry::WriteTextureCoordinates(command, textureCoordinates);
    if (positions[0] != 1.00000011920928955078125f || textureCoordinates[2] != 1.0f) {
        throw std::runtime_error("GX2 geometry or UV arithmetic rounded before vertex packing");
    }
#endif
    std::puts("PASS GX2 configured queue precision and final vertex/scissor packing");
}
