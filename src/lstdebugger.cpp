#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <cstdint>

#include "lstdebugger.hpp"

uint16_t lstAddrToVal(const std::string& strAddr) {
    uint16_t val = 0;
    for (size_t i = 0; i < strAddr.length(); i += 2) {
        std::string byteString = strAddr.substr(i, 2);
        uint8_t byte = std::stoul(byteString, nullptr, 16);
        val = (val * 256) + byte;
    }
    return val;
}

LstDebuggerAsm6::LstDebuggerAsm6(const std::string& lstfile, bool asm6) {
    std::ifstream file(lstfile);
    if (!file) {
        throw std::runtime_error("Unable to open file");
    }


    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] != '0') {
            continue;
        }
        uint16_t addr;
        std::string inst;
        if (asm6) {
            addr = lstAddrToVal(line.substr(1, 4));
            inst = line.substr(6);

        } else {
            addr = lstAddrToVal(line.substr(0, 5));
            addr -= 0x8000;
            inst = line.substr(11);
        }
        inst.erase(inst.find_last_not_of(" \n\r\t") + 1); // rstrip
        inst.erase(0, inst.find_first_not_of(" \n\r\t")); // lstrip

        if (!inst.empty()) {
            instMap[addr] = inst;
        }
    }
}

std::string LstDebuggerAsm6::getInst(uint16_t addr) const {
    auto it = instMap.find(addr);
    if (it != instMap.end()) {
        return it->second;
    }
    return "NOP";
}
