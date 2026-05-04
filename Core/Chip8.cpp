#include "Chip8.h"

#include <filesystem>
#include <ios>
#include <fstream>
#include <iostream>

Chip8::Chip8::Chip8()
    : generator(std::random_device{}()), distribution(0, 255) {
    reset();
}

bool Chip8::Chip8::loadRom(const std::string &path) {
    if (!std::filesystem::exists(path)) {
        std::cerr << "File does not exist" << std::endl;
    }
    std::ifstream file(path, std::ios::binary | std::ios::ate);

    if (!file) {
        std::cerr << "Bad file!!" << std::endl;
    }
    const std::streamsize size = file.tellg();

    if (size > 4096 - 0x200) {
        std::cerr << "File size limit exceeded" << std::endl;
    }

    file.seekg(0, std::ios::beg);

    // now read into memory
    if (file.read(reinterpret_cast<char*>(&memory[0x200]), size)) {
        std::cout << "Successfully loaded ROM: " << path << " (" << size << " bytes)" << std::endl;
        return true;
    }
    return false;
}

void Chip8::Chip8::tick() {
    const uint16_t instruction = (memory[pc] << 8 | memory[pc + 1]);
    pc +=2;

    // Deconstruct the instruction
    const uint8_t x = (instruction & 0x0F00) >> 8; // 2nd Nibble
    const uint8_t y = (instruction & 0x00F0) >> 4; // 3rd nibble
    const uint16_t n = instruction & 0x00F; // 4th nibble
    const uint8_t kk = (instruction & 0x00FF); // Last 8 bits (Last 2 nibbles)
    const uint16_t nnn = (instruction & 0x0FFF); // Last 12 bits (Last 3 nibbles) address

    // See: http://devernay.free.fr/hacks/chip8/C8TECH10.HTM#0.0
    switch (instruction & 0xF000) {
        case 0xF000:
            switch (instruction & 0x00FF) {
            case 0x0007: // Fx07: Set Vx = delay timer
                    vRegisters[x] = delayTimer;
                    break;

            case 0x000A: { // Fx0A: Wait for key press
                bool keyPressed = false;
                for (int i = 0; i < 16; ++i) {
                    if (kbState[i]) {
                        vRegisters[x] = static_cast<uint8_t>(i);
                        keyPressed = true;
                        break;
                    }
                }
                // If no key was pressed, repeat this instruction
                if (!keyPressed) {
                    pc -= 2;
                }
            } break;

            case 0x0015: // Fx15: Set delay timer = Vx
                    delayTimer = vRegisters[x];
                    break;

            case 0x0018: // Fx18: Set sound timer = Vx
                    soundTimer = vRegisters[x];
                    break;

            case 0x001E: // Fx1E: Set I = I + Vx
                    indexRegister += vRegisters[x];
                    break;

            case 0x0029: // Fx29: Set I = location of font sprite for Vx
                    // Each character is 5 bytes tall
                    indexRegister = vRegisters[x] * 5;
                    break;

            case 0x0033: // Fx33: Binary-coded decimal (BCD)
                    memory[indexRegister]     = vRegisters[x] / 100;
                    memory[indexRegister + 1] = (vRegisters[x] / 10) % 10;
                    memory[indexRegister + 2] = vRegisters[x] % 10;
                    break;

            case 0x0055: // Fx55: Store registers V0 through Vx in memory starting at I
                    for (int i = 0; i <= x; ++i) {
                        memory[indexRegister + i] = vRegisters[i];
                    }
                    break;

            case 0x0065: // Fx65: Read registers V0 through Vx from memory starting at I
                    for (int i = 0; i <= x; ++i) {
                        vRegisters[i] = memory[indexRegister + i];
                    }
                    break;

            default:
                    std::cerr << "Unknown F-series opcode: " << std::hex << instruction << std::endl;
                    break;
            }
            break;
        case 0x1000: // 0nnn
            pc = nnn; // Jump!
            break;
        case 0x6000: // LD Vx
            vRegisters[x] = kk;
            break;
        case 0x7000: // Add Vx
            vRegisters.at(x) += kk;
            break;
        case 0xA000: // Add
            indexRegister = nnn;
            break;
        case 0x0000: // CLR
            if (instruction == 0x00E0) {
                display.fill(0);
            } else if (instruction == 0x00EE) {
                // RET
                --sp;
                pc = stack[sp];
            }
            break;
        case 0xD000: { // Draw
            const uint8_t xPos = vRegisters[x];
            const uint8_t yPos = vRegisters[y];
            const uint8_t height = n;

            vRegisters[0xF] = 0;
            for (size_t row = 0; row < height; row++) {
                const uint8_t spriteByte = memory[indexRegister + row];
                for (size_t col = 0; col < 8; col++) {
                    if ((spriteByte & (0x80 >> col)) != 0) {
                        // bit idx is on
                        const uint8_t xCoord = (xPos + col);
                        const uint8_t yCoord = (yPos + row);

                        // Clip at edges instead of wrapping
                        if (xCoord < 64 && yCoord < 32) {
                            const uint16_t finalIdx = (yCoord * 64) + xCoord;
                            if (display[finalIdx] == 1) vRegisters[0xF] = 1;
                            display[finalIdx] ^= 1;
                        }
                    }
                }
            }
        } break;
        case 0xB000:
            pc = nnn + vRegisters[0];
            break;
        case 0x3000:
            if (vRegisters[x] == kk) pc +=2;
            break;
        case 0x4000:
            if (vRegisters[x] != kk) pc +=2;
            break;
        case 0x5000:
            if (vRegisters[x] == vRegisters[y]) pc +=2;
            break;
        case 0x9000:
            if (vRegisters[x] != vRegisters[y]) pc +=2;
            break;
        case 0xC000: {
            // Random
            std::uniform_int_distribution<uint8_t> distribution(0, 255);
            vRegisters[x] = distribution(generator) & kk;
        }
            break;
        case 0x2000:
            if (sp < 15) {
                stack[sp] = pc;
                sp++;
                pc = nnn;
            } break;
        case 0xE000: {
            switch (instruction & 0x00FF) {
                case 0x009E: { // skip if key pressed
                    if (const uint16_t key = vRegisters[x]; kbState[key]) {
                        pc += 2;
                    }
                } break;
                case 0x00A1: // skip if key not pressed
                    if (const uint16_t key = vRegisters[x]; !kbState[key]) {
                        pc +=2;
                    }
                    break;
                default:
                    std::cerr << "Unknown E-series opcode: " << std::hex << instruction << std::endl;
                    break;
            }
        } break;
        case 0x8000: {
            switch (instruction & 0x000F) {
                case 0x000: //8xy0
                    vRegisters[x] = vRegisters[y];
                    break;
                case 0x0001: // 8xy1
                    vRegisters[x] |= vRegisters[y];
                    break;
                case 0x0002: // 8xy2
                    vRegisters[x] = vRegisters[x] & vRegisters[y];
                    break;
                case 0x0003: // 8xy3
                    vRegisters[x] = vRegisters[x] ^ vRegisters[y];
                    break;
                case 0x0004: {
                    // 8xy4
                    const uint16_t sum = vRegisters[x] + vRegisters[y];
                    vRegisters[0xF] = (sum > 255) ? 1 : 0;
                    vRegisters[x] = static_cast<uint8_t>(sum & 0xFF);
                } break;
                case 0x0005: {
                    if (vRegisters[x] > vRegisters[y]) {
                        vRegisters[0xF] = 1;
                    } else vRegisters[0xF] = 0;
                    vRegisters[x] -= vRegisters[y];
                }
                    break;
                case 0x0006:
                    vRegisters[0xF] = (vRegisters[x] & 0x1);
                    vRegisters[x] = vRegisters[y] >> 1;
                    break;
                case 0x0007: //SUBN
                    (vRegisters[y] > vRegisters[x]) ? vRegisters[0xF] = 1 : vRegisters[0xF] = 0;
                    vRegisters[x] = vRegisters[y] - vRegisters[x];
                    break;
                case 0x000E:
                    vRegisters[0xF] = (vRegisters[x] & 0x80) >> 7;
                    vRegisters[x] = vRegisters[y] << 1;
                    break;
                default:
                    std::cerr << "Unknown 8-series opcode: " << std::hex << instruction << std::endl;
                    break;
            }
            break;
        }
        default:
        std::cerr << "Bad operation " << (instruction & 0xF000) << std::endl;
            break;
    }
}

void Chip8::Chip8::updateTimers() {
    if (delayTimer > 0) delayTimer--;
    if (soundTimer > 0) {
        soundTimer--;
    }
}

void Chip8::Chip8::reset() {
    memory.fill(0);
    vRegisters.fill(0);
    indexRegister = 0;

    sp = 0;
    delayTimer = 0;
    soundTimer = 0;

    pc = 0x200;

    for (std::size_t i = 0; i < fontSet.size(); i++) {
        memory.at(i) = fontSet.at(i);
    }
}