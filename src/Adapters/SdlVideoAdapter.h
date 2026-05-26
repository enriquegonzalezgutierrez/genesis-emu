// ==============================================================================
// GenesisEmu - SDL2 Video Adapter Header (Outer Hexagon - Updated with Scaling)
// ==============================================================================
// This class implements the Video Presentation Adapter. Added support for 
// hardware-accelerated integer scaling to support modern Full HD/4K monitors
// without losing crisp retro pixel quality.
// ==============================================================================

#pragma once

#include <SDL2/SDL.h>
#include <string>

namespace GenesisEmu::Adapters {

class SdlVideoAdapter {
public:
    // Constructor now takes logical (emulated) dimensions and a window scale factor (e.g., 4)
    SdlVideoAdapter(const std::string& title, int logicalWidth, int logicalHeight, int windowScale);
    
    // Destructor guarantees safe release of SDL2 hardware contexts (RAII)
    ~SdlVideoAdapter();

    // --------------------------------------------------------------------------
    // Public Control Interface
    // --------------------------------------------------------------------------
    // Initializes the SDL2 video subsystem, configures nearest-neighbor scaling
    // hints, creates the window, and sets up the hardware-accelerated renderer.
    bool Initialize();

    // Processes window events (keyboard inputs, window close buttons).
    bool ProcessEvents(int& offsetChange);

    // Takes a raw array of pixel data at native emulator resolution,
    // uploads it to the GPU, and upscales it dynamically to fill the larger window.
    void RenderFrame(const std::uint32_t* pixelData);

private:
    std::string m_title;
    
    // Native Sega Genesis emulated resolution
    int m_logicalWidth;
    int m_logicalHeight;
    
    // Actual host window resolution (scaled up)
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