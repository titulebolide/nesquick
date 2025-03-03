# pragma once

#include <iostream>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <cctype>
#include <cstdint>

class LstDebuggerAsm6 {
public:
    LstDebuggerAsm6(const std::string& lstfile);

    std::string getInst(uint16_t addr) const;

private:
    std::unordered_map<uint16_t, std::string> instMap;
};
