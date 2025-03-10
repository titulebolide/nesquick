#include <algorithm>
#include <stdexcept>

#include "cpumem.hpp"
/*
Memory map is a list of tuples
(startaddr, device)
From lowest startaddr to greatest

TODO : investigate the extent of this interface being the source of inefficiency
*/
Memory::Memory(const std::vector<std::pair<uint16_t, Device *>>& memory_map) {
    for (const auto& pair : memory_map) {
        mmap.push_back(pair);
    }
    // reverse the list to ease device search
    // search from the biggest addr and stop at the
    // first one lowest than the addr were looking for
    std::reverse(mmap.begin(), mmap.end());
}

uint8_t Memory::get(uint16_t index) {
    for (auto& pair : mmap) {
        if (index >= pair.first) {
            return pair.second->get(index);
        }
    }
    throw std::runtime_error("Bad memory map");
}

void Memory::set(uint16_t index, uint8_t value) {
    for (auto& pair : mmap) {
        if (index >= pair.first) {
            pair.second->set(index, value);
            return;
        }
    }
    throw std::runtime_error("Bad memory map");
}
