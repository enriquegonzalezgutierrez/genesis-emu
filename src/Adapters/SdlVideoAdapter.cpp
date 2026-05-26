// ==============================================================================
// GenesisEmu - SDL2 Video Adapter Implementation (Outer Hexagon)
// ==============================================================================
// This file implements the window creation, GPU texture mapping, and event
// handling loop using the SDL2 library.
// ==============================================================================

#include "SdlVideoAdapter.h"
#include <iostream>

namespace GenesisEmu::Adapters {

SdlVideoAdapter::SdlVideoAdapter(const std::string& title, int width, int height)
    : m_title(title), m_width(width), m_height(height),
      m_window(nullptr), m_renderer(nullptr), m_texture(nullptr) {}

SdlVideoAdapter::~SdlVideoAdapter() {
    // Safely destroy resources in reverse allocation order
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
    }
    
    // Shut down the SDL Video Subsystem
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::cout << "[SDL] Video subsystem shut down successfully." << std::endl;
}

// ------------------------------------------------------------------------------
// Hardware Initialization
// ------------------------------------------------------------------------------
bool SdlVideoAdapter::Initialize() {
    // 1. Initialize the SDL Video subsystem
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[SDL Error] Failed to init Video subsystem: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    // 2. Create the window
    m_window = SDL_CreateWindow(
        m_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "[SDL Error] Failed to create window: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    // 3. Create Hardware-Accelerated Renderer linked to the GTX 1060
    // SDL_RENDERER_PRESENTVSYNC locks the frame rate to the monitor's refresh rate (60Hz)
    m_renderer = SDL_CreateRenderer(
        m_window, 
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        std::cerr << "[SDL Error] Failed to create GPU renderer: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    // 4. Create Streaming Texture
    // STREAMING access allows the CPU to quickly update pixel arrays and upload them to VRAM
    // RGBA8888 uses 32 bits per pixel (Red, Green, Blue, Alpha)
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_width,
        m_height
    );

    if (!m_texture) {
        std::cerr << "[SDL Error] Failed to create streaming texture: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "[SDL] Window and GPU streaming texture initialized successfully." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// Event Loop Processing
// ------------------------------------------------------------------------------
bool SdlVideoAdapter::ProcessEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false; // User closed the window
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                return false; // ESC key exits the emulator
            }
        }
    }
    return true;
}

// ------------------------------------------------------------------------------
// Frame Rendering Engine
// ------------------------------------------------------------------------------
void SdlVideoAdapter::RenderFrame(const std::uint32_t* pixelData) {
    if (!m_renderer || !m_texture || !pixelData) return;

    // 1. Upload raw pixel array from system memory to the GPU VRAM Texture
    // m_width * sizeof(uint32_t) is the pitch (bytes per row: 320 * 4 = 1280 bytes)
    SDL_UpdateTexture(m_texture, nullptr, pixelData, m_width * sizeof(std::uint32_t));

    // 2. Clear current screen buffer
    SDL_RenderClear(m_renderer);

    // 3. Copy the updated VRAM texture onto the renderer's backbuffer
    // Passing nullptr to src/dst rects stretches the texture to fill the window size dynamically
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);

    // 4. Swap buffers (present the rendered frame to the screen)
    SDL_RenderPresent(m_renderer);
}

} // namespace GenesisEmu::Adapters