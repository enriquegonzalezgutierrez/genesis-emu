// ==============================================================================
// GenesisEmu - VDP Consolidated Graphics Renderer (Core Domain)
// ==============================================================================
// This class implements the graphics composition engine. It parses Scroll 
// Planes (Plane A, Plane B) and Sprites directly from VRAM/CRAM according 
// to VDP register layouts.
//
// DESIGN STRATEGY:
// 1. High Performance: Directly compiles 9-bit Sega BGR colors to 32-bit RGBA.
// 2. SRP: Separates GPU output formatting from VRAM memory access boundaries.
// ==============================================================================

#pragma once

#include "Vdp.h"
#include <cstdint>

namespace GenesisEmu::Core {

/**
 * @class VdpRenderer
 * @brief Handles real-time background planes composition and sprite rendering.
 */
class VdpRenderer {
public:
    VdpRenderer() = delete;
    ~VdpRenderer() = delete;

    /**
     * @brief Translates a Sega Genesis 9-bit BGR color format to a 32-bit RGBA color.
     *        Sega format: 0000 BBB0 GGG0 RRR0 (3 bits per color component, scaled to 8-bit).
     * @param cramHigh High byte from CRAM.
     * @param cramLow Low byte from CRAM.
     * @return 32-bit RGBA color representation.
     */
    static std::uint32_t ConvertColor(Byte cramHigh, Byte cramLow);

    /**
     * @brief Renders a complete horizontal scanline of a Scroll Plane (A or B).
     * @param vdp Reference to the VDP.
     * @param planeIndex 0 for Plane A, 1 for Plane B.
     * @param scanline The vertical pixel line index currently rendering (0 to 223).
     * @param screenWidth Width of the destination screen buffer (320).
     * @param lineBuffer Pointer to the target scanline memory in the framebuffer.
     */
    static void RenderPlaneScanline(const Vdp& vdp, int planeIndex, int scanline, 
                                    int screenWidth, std::uint32_t* lineBuffer);

    /**
     * @brief Renders active active sprites on the specified scanline (Priority layer).
     * @param vdp Reference to the VDP.
     * @param scanline The current rendering scanline.
     * @param screenWidth Width of the destination screen buffer.
     * @param lineBuffer Pointer to the target scanline memory in the framebuffer.
     */
    static void RenderSpritesScanline(const Vdp& vdp, int scanline, 
                                      int screenWidth, std::uint32_t* lineBuffer);
};

} // namespace GenesisEmu::Core