#ifndef CHIP8_CHIP8_H
#define CHIP8_CHIP8_H
#include <array>
#include <cstdint>
#include <string>
#include <random>

namespace Chip8 {
    class Chip8 {
    public:

        Chip8();

        std::array<uint8_t, 4096> memory;
        std::array<uint8_t, 16> vRegisters; // V0 - VF
        std::array<uint16_t, 16> stack;
        std::array<bool, 16> kbState;

        uint16_t indexRegister;
        uint16_t pc;
        uint8_t sp;
        uint8_t delayTimer;
        uint8_t soundTimer;

        // GFX
        std::array<uint32_t, 64 * 32> display;

        static constexpr std::array<uint8_t, 80> fontSet = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        };

        void reset();
        void tick();
        void updateTimers();
        bool loadRom(const std::string& path);
    private:
        std::mt19937 generator;
        std::uniform_int_distribution<uint8_t> distribution;

    };

}

#endif
