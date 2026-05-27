// ==============================================================================
// GenesisEmu - SDL2 Video and Input Adapter Header (Outer Hexagon)
// ==============================================================================
// This file declares the SdlVideoAdapter class. It handles host-level rendering
// (GPU upscaling via SDL textures) and polling of input events.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for host presentation and keyboard state polling.
// 2. Dependency Inversion Principle (DIP):
//    It maps host key inputs directly to the abstract IoPorts domain entity,
//    preventing GUI dependencies from slipping into the core logic.
// ==============================================================================

#pragma once

#include <SDL2/SDL.h>
#include <string>
#include "../Core/Domain/Io/IoPorts.h" // Input registers bridge

namespace GenesisEmu::Adapters {

/**
 * @class SdlVideoAdapter
 * @brief Graphical presentation and host keyboard input polling adapter.
 */
class SdlVideoAdapter {
public:
    SdlVideoAdapter(const std::string& title, int logicalWidth, int logicalHeight, int windowScale);
    ~SdlVideoAdapter();

    // --- Public Adapter Interface ---

    /**
     * @brief Instantiates the window, GPU renderer, and streaming texture interfaces.
     * @return True on success, false on initialization errors.
     */
    bool Initialize();

    /**
     * @brief Polls host keyboard states, translating them directly to Sega controller pins.
     * @param ioPorts Reference to the Core domain IoPorts register entity.
     * @return False if the host window is closed or ESC key is triggered.
     */
    bool ProcessEvents(Core::Domain::Io::IoPorts& ioPorts);

    /**
     * @brief Uploads native 320x224 RGBA framebuffers to the GPU for scaled rendering.
     * @param pixelData Pointer to the start of the 32-bit pixel array.
     */
    void RenderFrame(const std::uint32_t* pixelData);

private:
    std::string m_title;
    
    // Native Sega Genesis resolution
    int m_logicalWidth;
    int m_logicalHeight;
    
    // Scaled physical window dimensions
    int m_windowWidth;
    int m_windowHeight;

    // Direct SDL GPU Interface handles
    SDL_Window*   m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture*  m_texture;

    // Prevent copies to avoid double-free of active SDL hardware handles
    SdlVideoAdapter(const SdlVideoAdapter&) = delete;
    SdlVideoAdapter& operator=(const SdlVideoAdapter&) = delete;
};

} // namespace GenesisEmu::Adapters