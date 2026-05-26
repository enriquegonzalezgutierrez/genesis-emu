// ==============================================================================
// GenesisEmu - SDL2 Video Adapter Header (Outer Hexagon)
// ==============================================================================
// This class implements the Video Presentation Adapter. It manages the physical
// window, GPU renderer, and video texture using SDL2. It is completely decoupled
// from the Core emulator logic.
// ==============================================================================

#pragma once

#include <SDL2/SDL.h>
#include <string>

namespace GenesisEmu::Adapters {

class SdlVideoAdapter {
public:
    // Constructor defines window properties but does not allocate hardware resources yet
    SdlVideoAdapter(const std::string& title, int width, int height);
    
    // Destructor guarantees safe release of SDL2 hardware contexts (RAII)
    ~SdlVideoAdapter();

    // --------------------------------------------------------------------------
    // Public Control Interface
    // --------------------------------------------------------------------------
    // Initializes the SDL2 video subsystem, creates the window, and sets up
    // the hardware-accelerated 2D renderer. Returns true on success.
    bool Initialize();

    // Processes window events (keyboard inputs, window close buttons).
    // Returns false if the user requested to close the application.
    bool ProcessEvents();

    // Takes a raw array of pixel data (format: RGBA, 32-bit per pixel),
    // uploads it directly to the GTX 1060 VRAM, and presents it to the monitor.
    void RenderFrame(const std::uint32_t* pixelData);

private:
    std::string m_title;
    int m_width;
    int m_height;

    // SDL2 Hardware handles
    SDL_Window*   m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture*  m_texture;

    // Prevent copy constructor and assignment operator to avoid double-free of SDL handles
    SdlVideoAdapter(const SdlVideoAdapter&) = delete;
    SdlVideoAdapter& operator=(const SdlVideoAdapter&) = delete;
};

} // namespace GenesisEmu::Adapters