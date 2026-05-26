// ==============================================================================
// GenesisEmu - SDL2 Video & Input Adapter Header (Outer Hexagon)
// ==============================================================================
// Adds hardware-accelerated upscaling and maps real-world host keyboard 
// events to the emulated Front Controller Ports.
// ==============================================================================

#pragma once

#include <SDL2/SDL.h>
#include <string>
#include "IoPorts.h" // Holds physical Gamepad state mappings

namespace GenesisEmu::Adapters {

class SdlVideoAdapter {
public:
    // Logical (emulated) dimensions and a scale factor (e.g., 320x224 scaled 4x)
    SdlVideoAdapter(const std::string& title, int logicalWidth, int logicalHeight, int windowScale);
    ~SdlVideoAdapter();

    // --- Public Control Interface ---
    bool Initialize();

    /**
     * @brief Polls SDL window events and maps physical keys directly to the IoPorts state.
     * @param ioPorts Reference to the core IoPorts device to update.
     * @return False if the window is closed or ESC is pressed.
     */
    bool ProcessEvents(Core::IoPorts& ioPorts);

    // Takes a raw array of pixel data at native emulator resolution (320x224),
    // uploads it to the GPU, and upscales it dynamically.
    void RenderFrame(const std::uint32_t* pixelData);

private:
    std::string m_title;
    
    // Native Sega Genesis emulated resolution
    int m_logicalWidth;
    int m_logicalHeight;
    
    // Actual host window resolution
    int m_windowWidth;
    int m_windowHeight;

    // SDL2 Hardware handles
    SDL_Window*   m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture*  m_texture;

    // Prevent copy constructor and assignment operator to avoid double-free of SDL handles
    SdlVideoAdapter(const SdlVideoAdapter&) = delete;
    SdlVideoAdapter& operator=(const SdlVideoAdapter&) = delete;
};

} // namespace GenesisEmu::Adapters