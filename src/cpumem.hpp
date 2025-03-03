#pragma once

#include <vector>
#include <cstdint>

#include "device.hpp"

class Memory {
 public:
    Memory(const std::vector<std::pair<uint16_t, Device*>>& memory_map);

    uint8_t get(uint16_t index);
    void set(uint16_t index, uint8_t value);

 private:
    std::vector<std::pair<uint16_t, Device*>> mmap;
};
