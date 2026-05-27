// ==============================================================================
// GenesisEmu - SDL2 Video and Input Adapter Implementation (Outer Hexagon)
// ==============================================================================
// This file implements host graphical presentation and active-low input bindings.
//
// SOLID Compliance:
// 1. Single Responsibility Principle (SRP):
//    It is strictly responsible for host-level OS event mappings and GPU texturing.
// ==============================================================================

#include "SdlVideoAdapter.h"
#include <iostream>

namespace GenesisEmu::Adapters {

using namespace GenesisEmu::Core::Domain::Io;

SdlVideoAdapter::SdlVideoAdapter(const std::string& title, int logicalWidth, int logicalHeight, int windowScale)
    : m_title(title)
    , m_logicalWidth(logicalWidth)
    , m_logicalHeight(logicalHeight)
    , m_windowWidth(logicalWidth * windowScale)
    , m_windowHeight(logicalHeight * windowScale)
    , m_window(nullptr)
    , m_renderer(nullptr)
    , m_texture(nullptr) 
{}

SdlVideoAdapter::~SdlVideoAdapter() {
    // Safely free hardware texture and presentation contexts
    if (m_texture)  SDL_DestroyTexture(m_texture);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window)   SDL_DestroyWindow(m_window);
    
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::cout << "[SDL] Video and Input presentation adapters shut down." << std::endl;
}

bool SdlVideoAdapter::Initialize() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[SDL Error] Failed to initialize Video Subsystem: " << SDL_GetError() << std::endl;
        return false;
    }

    // Configure nearest-neighbor scaling (pixelated look, zero smoothing filters)
    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) {
        std::cerr << "[SDL Warning] Nearest-neighbor hint rejected by host graphics driver." << std::endl;
    }

    m_window = SDL_CreateWindow(
        m_title.c_str(),
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "[SDL Error] Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create accelerated, VSync-enabled renderer context
    m_renderer = SDL_CreateRenderer(
        m_window, 
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        std::cerr << "[SDL Error] Failed to create GPU context: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create streaming texture matching the standard RGBA8888 32-bit pixel layout
    m_texture = SDL_CreateTexture(
        m_renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        m_logicalWidth,
        m_logicalHeight
    );

    if (!m_texture) {
        std::cerr << "[SDL Error] Failed to create streaming texture: " << SDL_GetError() << std::endl;
        return false;
    }

    std::cout << "[SDL] Graphical presentation context built: " 
              << m_logicalWidth << "x" << m_logicalHeight << " -> Scaled to: "
              << m_windowWidth << "x" << m_windowHeight << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// Key Polling Loop with Sega Gamepad Pin Translation
// ------------------------------------------------------------------------------
bool SdlVideoAdapter::ProcessEvents(IoPorts& ioPorts) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool pressed = (event.type == SDL_KEYDOWN);
            
            switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    if (pressed) return false;
                    break;
                    
                // Directionals
                case SDLK_UP:
                    ioPorts.SetButtonState(GamepadButton::UP, pressed);
                    break;
                case SDLK_DOWN:
                    ioPorts.SetButtonState(GamepadButton::DOWN, pressed);
                    break;
                case SDLK_LEFT:
                    ioPorts.SetButtonState(GamepadButton::LEFT, pressed);
                    break;
                case SDLK_RIGHT:
                    ioPorts.SetButtonState(GamepadButton::RIGHT, pressed);
                    break;
                    
                // Action Buttons (A, B, C, START)
                case SDLK_z:
                    ioPorts.SetButtonState(GamepadButton::A, pressed);
                    break;
                case SDLK_x:
                    ioPorts.SetButtonState(GamepadButton::B, pressed);
                    break;
                case SDLK_c:
                    ioPorts.SetButtonState(GamepadButton::C, pressed);
                    break;
                case SDLK_RETURN:
                    ioPorts.SetButtonState(GamepadButton::START, pressed);
                    break;
                    
                default:
                    break;
            }
        }
    }
    return true;
}

void SdlVideoAdapter::RenderFrame(const std::uint32_t* pixelData) {
    if (!m_renderer || !m_texture || !pixelData) return;

    // Lock and upload raw pixels to GPU texture
    SDL_UpdateTexture(m_texture, nullptr, pixelData, m_logicalWidth * sizeof(std::uint32_t));
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
}

} // namespace GenesisEmu::Adapters