#include <iostream>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>

#include "Core/Chip8.h"

struct SDLWindowDeleter {
    void operator()(SDL_Window* window) const {SDL_DestroyWindow(window); }
};

struct SDLRendererDeleter {
    void operator()(SDL_Renderer* renderer) const {SDL_DestroyRenderer(renderer); }
};

using UniqueWindow = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
using UniqueRenderer = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;

int main() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
        return -1;
    }

    constexpr SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, 44100};
    SDL_AudioStream* stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    SDL_ResumeAudioDevice(SDL_GetAudioStreamDevice(stream));



    const UniqueWindow window(SDL_CreateWindow("Chip8 Emulator", 1280, 720, SDL_WINDOW_RESIZABLE));
    if (!window) {return -1; }

    const UniqueRenderer renderer(SDL_CreateRenderer(window.get(), ""));
    if (!renderer) {return -1; }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    ImGui_ImplSDL3_InitForSDLRenderer(window.get(), renderer.get());
    ImGui_ImplSDLRenderer3_Init(renderer.get());

    Chip8::Chip8 cpu{};
    cpu.reset();
    uint64_t lastTime = SDL_GetTicks();
    double accumulator = 0.0;
    const double targetHz = 500.0;
    const double timePerTick = 1000.0 / targetHz;

    const bool romLoaded = cpu.loadRom("../Space Invaders [David Winter].ch8");

    SDL_Texture* chip8Texture = SDL_CreateTexture(renderer.get(),
        SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 64, 32);
    SDL_SetTextureScaleMode(chip8Texture, SDL_SCALEMODE_NEAREST);

    // Create a 0.1 second beep buffer
    constexpr int sampleRate = 44100;
    constexpr int numSamples = sampleRate / 10; // 0.1 seconds
    std::vector<float> beepSamples(numSamples);

    for (int i = 0; i < numSamples; ++i) {
        // Generate a simple square wave at 440Hz (Pitch A)
        // If the sine is positive, set to 0.2 volume, else -0.2
        const float time = static_cast<float>(i) / sampleRate;
        beepSamples[i] = (sinf(2.0f * M_PI * 440.0f * time) > 0) ? 0.2f : -0.2f;
    }


    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);

            if (e.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                e.window.windowID == SDL_GetWindowID(window.get())) {
                running = false;
            }

            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_1: cpu.kbState[0x1] = true; break;
                    case SDLK_2: cpu.kbState[0x2] = true; break;
                    case SDLK_Q: cpu.kbState[0x4] = true; break;
                    case SDLK_W: cpu.kbState[0x5] = true; break;
                    case SDLK_E: cpu.kbState[0x6] = true; break;
                    default:
                        break;
                }
            }
            if (e.type == SDL_EVENT_KEY_UP) {
                switch (e.key.key) {
                    case SDLK_1: cpu.kbState[0x1] = false; break;
                    case SDLK_2: cpu.kbState[0x2] = false; break;
                    case SDLK_Q: cpu.kbState[0x4] = false; break;
                    case SDLK_W: cpu.kbState[0x5] = false; break;
                    case SDLK_E: cpu.kbState[0x6] = false; break;
                    default:
                        break;
                }
            }
        }

        if (cpu.soundTimer > 0) {
            // If the stream is getting empty, put more "beep" in it
            if (SDL_GetAudioStreamQueued(stream) < (int)(beepSamples.size() * sizeof(float))) {
                SDL_PutAudioStreamData(stream, beepSamples.data(), beepSamples.size() * sizeof(float));
            }
        } else {
            // Optional: Clear the stream so the beep stops immediately
            SDL_ClearAudioStream(stream);
        }
        if (cpu.soundTimer > 0) {
            // If the stream is getting empty, put more "beep" in it
            if (SDL_GetAudioStreamQueued(stream) < static_cast<int>(beepSamples.size() * sizeof(float))) {
                SDL_PutAudioStreamData(stream, beepSamples.data(), beepSamples.size() * sizeof(float));
            }
        } else {
            // Optional: Clear the stream so the beep stops immediately
            SDL_ClearAudioStream(stream);
        }

        const uint64_t currentTime = SDL_GetTicks();
        const auto deltaTime = static_cast<double>(currentTime - lastTime);
        lastTime = currentTime;
        accumulator += deltaTime;

        while (accumulator >= timePerTick) {
            cpu.tick();
            accumulator -= timePerTick;
        }

        static double timerAccumulator = 0.0;
        timerAccumulator += deltaTime;

        if (timerAccumulator >= (1000.0 / 60.0)) {
            cpu.updateTimers();
            timerAccumulator = 0.0;
        }

        std::vector<uint32_t> pixelBuffer(64 * 32);
        for (size_t i = 0; i < 64 * 32; i++) {
            pixelBuffer[i] = (cpu.display[i] == 1) ? 0xFFFFFFFF : 0x000000FF;
        }

        SDL_UpdateTexture(chip8Texture, nullptr, pixelBuffer.data(), 64 * sizeof(uint32_t));

        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Game View");
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        ImGui::Image((ImTextureID)chip8Texture, viewportSize);
        ImGui::End();

        ImGui::Begin("CPU Debugger");
        if (romLoaded) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Rom Loaded");
        } else {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "No ROM Loaded");
        }

        ImGui::SeparatorText("Keypad State");
        for (int i = 0; i < 16; i++) {
            if (cpu.kbState[i]) {
                ImGui::Text("Key %X: [PRESSED]", i);
            }
        }

        ImGui::SeparatorText("Registers");
        for (size_t i = 0; i  < 16; i++) {
            ImGui::Text("V%X: 0x%02X", i, cpu.vRegisters[i]);
            if (i % 4 != 3) ImGui::SameLine(ImGui::GetWindowWidth() * ((i % 4 + 1) * 0.25f));
        }

        ImGui::SeparatorText("Memory (at 0x200)");
        ImGui::BeginChild("MemRegion", ImVec2(0, 200), true);
        for (int i = 0x200; i < 0x200 + 64; i++) { // Show first 64 bytes of ROM
            ImGui::Text("%02X", cpu.memory[i]);
            if ((i + 1) % 8 != 0) ImGui::SameLine();
        }
        ImGui::EndChild();

        ImGui::End();

        ImGui::Begin("Chip8 Emulator", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Average FPS: %.1f / FPS: %.1f", 1000.0f / io.Framerate, io.Framerate);

        static float emu_speed = 500.0f;
        ImGui::SliderFloat("Speed", &emu_speed, 0.0f, 1.0f, "%.2f");

        if (ImGui::Button("Reset")) {
            running = false;
        }

        ImGui::End();
        ImGui::Render();

        SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 255);
        SDL_RenderClear(renderer.get());

        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer.get());
        SDL_RenderPresent(renderer.get());
    }

    SDL_DestroyAudioStream(stream);
    return 0;
}

