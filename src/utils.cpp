#include <iostream>
#include <fstream>
#include <vector>
#include <stdexcept>
#include <cstdint>
#include <iomanip>
#include <bitset>
#include <thread>
#include <chrono>
#include "utils.hpp"

// Utility functions
uint8_t byte_not(uint8_t val) {
    return ~val;
}

void sleep(int sec) {
    std::this_thread::sleep_for(std::chrono::seconds(sec));
}

std::string dec2hex(uint16_t val) {
    std::stringstream ss;
    ss << std::hex << std::setw(2) << std::setfill('0') << val;
    return ss.str();
}

uint8_t low_byte(uint16_t val) {
    return val & 0xff;
}

uint8_t high_byte(uint16_t val) {
    return val >> 8;
}

std::string bin8(uint8_t val) {
    return std::bitset<8>(val).to_string();
}

std::string hex2(uint8_t val) {
    std::stringstream ss;
    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(val);
    return ss.str();
}

std::string binstr(uint8_t value) {
    std::stringstream ss;
    ss << std::bitset<8>{value};
    return ss.str();
}

std::string hexstr(uint8_t value) {
    std::stringstream ss;
    ss << std::uppercase << std::setw(2) << std::setfill('0')
       << std::hex << static_cast<int>(value);
    return ss.str();
}

std::string hexstr(uint16_t value) {
    std::stringstream ss;
    ss << std::uppercase << std::setw(4) << std::setfill('0')
       << std::hex << static_cast<int>(value);
    return ss.str();
}

void parseInes(const std::string& filename, uint8_t * prg, uint8_t * chr, uint16_t * prgLen, uint16_t * chrLen) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    if (data.size() < 16 || data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A) {
        throw std::runtime_error("Bad file header");
    }

    std::vector<uint8_t> header(data.begin(), data.begin() + 16);
    *prgLen = header[4] * 16384;
    *chrLen = header[5] * 8192;

    if (data.size() != *chrLen + *prgLen + 16) {
        throw std::runtime_error("Unsupported file format");
    }
    for (int addr = 0; addr < *prgLen; addr ++) {
        prg[addr] = data[16 + addr];
    }
    for (int addr = 0; addr < *chrLen; addr ++) {
        chr[addr] = data[16 + *prgLen + addr];
    }
}
