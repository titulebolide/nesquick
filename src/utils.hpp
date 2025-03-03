#pragma once

#include <cctype>
#include <string>

uint8_t byte_not(uint8_t val);
std::string dec2hex(uint16_t val);
uint8_t low_byte(uint16_t val);
uint8_t high_byte(uint16_t val);
std::string bin8(uint8_t val);
std::string hex2(uint8_t val);
uint16_t lstAddrToVal(const std::string& strAddr);
std::string hexstr(uint8_t value);
void parseInes(const std::string& filename, uint8_t * prg, uint8_t * chr);
