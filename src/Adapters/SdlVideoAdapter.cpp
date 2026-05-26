// ==============================================================================
// GenesisEmu - SDL2 Video Adapter Implementation (Outer Hexagon - Updated)
// ==============================================================================
// This file implements the scaled window creation, GPU texture mapping, and event
// handling loop. Configures nearest-neighbor scaling for sharp retro pixels.
// ==============================================================================

#include "SdlVideoAdapter.h"
#include <iostream>

namespace GenesisEmu::Adapters {

SdlVideoAdapter::SdlVideoAdapter(const std::string& title, int logicalWidth, int logicalHeight, int windowScale)
    : m_title(title), m_logicalWidth(logicalWidth), m_logicalHeight(logicalHeight),
      m_windowWidth(logicalWidth * windowScale), m_windowHeight(logicalHeight * windowScale),
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
// Hardware Initialization with Upscaling Configuration
// ------------------------------------------------------------------------------
bool SdlVideoAdapter::Initialize() {
    // 1. Initialize the SDL Video subsystem
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[SDL Error] Failed to init Video subsystem: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    // 2. Set Scaling Quality Hint to NEAREST-NEIGHBOR ("0")
    // This disables bilinear filtering, keeping the upscaled pixel-art crisp and sharp
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) {
        std::cerr << "[SDL Warning] Failed to set nearest-neighbor rendering scale quality." << std::endl;
    }

    // 3. Create the window at upscaled resolution (e.g., 1280x896)
    m_window = SDL_CreateWindow(
        m_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "[SDL Error] Failed to create window: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    // 4. Create Hardware-Accelerated Renderer linked to the GPU
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

    // 5. Create Streaming Texture AT LOGICAL EMULATOR RESOLUTION (e.g., 320x224)
    // The GPU will automatically scale this small texture to fill the large window during render
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_logicalWidth,
        m_logicalHeight
    );

    if (!m_texture) {
        std::cerr << "[SDL Error] Failed to create streaming texture: " 
                  << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "[SDL] Window (" << m_windowWidth << "x" << m_windowHeight 
              << ") and GPU texture (" << m_logicalWidth << "x" << m_logicalHeight 
              << ") initialized with nearest-neighbor scaling." << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// Event Loop Processing with Keyboard Hook
// ------------------------------------------------------------------------------
bool SdlVideoAdapter::ProcessEvents(int& offsetChange) {
    offsetChange = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false; // User closed the window
        }
        
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    return false; // ESC key exits the emulator
                    
                case SDLK_UP:
                    offsetChange = 32768; // Go forward 32 KB
                    break;
                    
                case SDLK_DOWN:
                    offsetChange = -32768; // Go backward 32 KB
                    break;
                    
                default:
                    break;
            }
        }
    }
    return true;
}

// ------------------------------------------------------------------------------
// Frame Rendering Engine with Automatic Scaling
// ------------------------------------------------------------------------------
void SdlVideoAdapter::RenderFrame(const std::uint32_t* pixelData) {
    if (!m_renderer || !m_texture || !pixelData) return;

    // 1. Upload raw pixel array from system memory to the VRAM texture (320x224)
    SDL_UpdateTexture(m_texture, nullptr, pixelData, m_logicalWidth * sizeof(std::uint32_t));

    // 2. Clear current screen buffer
    SDL_RenderClear(m_renderer);

    // 3. Copy and stretch the native texture to fill the larger window dynamically
    // The GPU handles this stretching instantly using the nearest-neighbor hint
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);

    // 4. Swap buffers (present the rendered frame to the screen)
    SDL_RenderPresent(m_renderer);
}

} // namespace GenesisEmu::Adapters