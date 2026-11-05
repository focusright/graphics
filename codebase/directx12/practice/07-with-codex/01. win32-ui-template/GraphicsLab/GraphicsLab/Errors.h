#pragma once

#include <windows.h>

#include <format>
#include <stdexcept>

inline void Check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        throw std::runtime_error(std::format("{} failed (HRESULT 0x{:08X})", operation,
            static_cast<unsigned int>(result)));
    }
}

inline void Require(bool condition, const char* operation) {
    if (!condition) {
        throw std::runtime_error(operation);
    }
}
