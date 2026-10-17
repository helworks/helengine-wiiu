#include "platform/wiiu/WiiUGx2QuadGeometry.hpp"
#include <cmath>
#include <stdexcept>
#include <cstdio>

/// Verifies signed/sheared sprite corners retain their UV triangle indices under destination projection.
int main() {
    helengine::wiiu::WiiUGx2QuadCommand command {};
    command.HasResolvedCorners = true;
    command.CornerX = {80, 0, 20, 100};
    command.CornerY = {10, 30, 90, 70};
    float positions[12];
    helengine::wiiu::WiiUGx2QuadGeometry::WritePositions(command, 200, 100, positions);
    const int corners[6] = {3, 2, 1, 3, 1, 0};
    for (int i = 0; i < 6; ++i) {
        if (std::abs(positions[i * 2] - (command.CornerX[corners[i]] / 100 - 1)) > 1e-6
            || std::abs(positions[i * 2 + 1] - (1 - command.CornerY[corners[i]] / 50)) > 1e-6) {
            throw std::runtime_error("GX2 projected corner escaped its associated UV triangle index");
        }
    }
    command.HasResolvedCorners = false;
    command.X = 10; command.Y = 20; command.Width = 30; command.Height = 40;
    helengine::wiiu::WiiUGx2QuadGeometry::WritePositions(command, 200, 100, positions);
    if (std::abs(positions[0] + 0.9f) > 1e-6 || std::abs(positions[1] + 0.2f) > 1e-6) {
        throw std::runtime_error("GX2 glyph/solid rectangle projection changed");
    }
    std::puts("PASS GX2 exact-corner triangle projection and legacy rectangle projection");
}
