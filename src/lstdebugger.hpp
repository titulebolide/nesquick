# pragma once

#include <string>
#include <unordered_map>
#include <cstdint>

class LstDebuggerAsm6 {
public:
    LstDebuggerAsm6(const std::string& lstfile, bool asm6);

    std::string getInst(uint16_t addr) const;

private:
    std::unordered_map<uint16_t, std::string> instMap;
};
