// ==============================================================================
// GenesisEmu - SDL2 Video & Input Adapter Implementation (Outer Hexagon)
// ==============================================================================
// This file implements the scaled window creation, GPU texture mapping, and 
// physical key polling translation using the Select-Line IoPorts layout.
// ==============================================================================

#include "SdlVideoAdapter.h"
#include <iostream>

namespace GenesisEmu::Adapters {

using namespace GenesisEmu::Core;

SdlVideoAdapter::SdlVideoAdapter(const std::string& title, int logicalWidth, int logicalHeight, int windowScale)
    : m_title(title), m_logicalWidth(logicalWidth), m_logicalHeight(logicalHeight),
      m_windowWidth(logicalWidth * windowScale), m_windowHeight(logicalHeight * windowScale),
      m_window(nullptr), m_renderer(nullptr), m_texture(nullptr) {}

SdlVideoAdapter::~SdlVideoAdapter() {
    if (m_texture) SDL_DestroyTexture(m_texture);
    if (m_renderer) SDL_DestroyRenderer(m_renderer);
    if (m_window) SDL_DestroyWindow(m_window);
    
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::cout << "[SDL] Video and Input subsystems shut down." << std::endl;
}

bool SdlVideoAdapter::Initialize() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[SDL Error] Failed to init Video: " << SDL_GetError() << std::endl;
        return false;
    }

    if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0")) {
        std::cerr << "[SDL Warning] Nearest-neighbor hint rejected." << std::endl;
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

    m_renderer = SDL_CreateRenderer(
        m_window, 
        -1, 
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        std::cerr << "[SDL Error] Failed to create GPU renderer: " << SDL_GetError() << std::endl;
        return false;
    }

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

    std::cout << "[SDL] Video subsystem initialized. Logical: " 
              << m_logicalWidth << "x" << m_logicalHeight << " -> Scaled: "
              << m_windowWidth << "x" << m_windowHeight << std::endl;
    return true;
}

// ------------------------------------------------------------------------------
// Key Polling Loop with Dual Phase Translation
// ------------------------------------------------------------------------------
bool SdlVideoAdapter::ProcessEvents(Core::IoPorts& ioPorts) {
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
                    
                // Up, Down, Left, Right directional mapping
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
                    
                // Sega standard face buttons (A, B, C, START)
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

    SDL_UpdateTexture(m_texture, nullptr, pixelData, m_logicalWidth * sizeof(std::uint32_t));
    SDL_RenderClear(m_renderer);
    SDL_RenderCopy(m_renderer, m_texture, nullptr, nullptr);
    SDL_RenderPresent(m_renderer);
}

} // namespace GenesisEmu::Adapters