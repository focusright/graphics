#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <string>

struct DxException {
    HRESULT hr;
    std::wstring func;
    std::wstring file;
    int line;
    std::wstring ToString() const;
};

inline std::wstring AnsiToWide(const char* str) {
    if (!str || !*str) return std::wstring();
    int n = MultiByteToWideChar(CP_ACP, 0, str, -1, nullptr, 0);
    std::wstring out(n, 0);
    MultiByteToWideChar(CP_ACP, 0, str, -1, &out[0], n);
    if (!out.empty() && out.back() == 0) out.pop_back();
    return out;
}

#define DX_CHECK(x) do { HRESULT hr__ = (x); if (FAILED(hr__)) throw DxException{ hr__, L#x, AnsiToWide(__FILE__), __LINE__ }; } while(0)
