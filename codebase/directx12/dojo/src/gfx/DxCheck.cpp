#include "DxCheck.h"
#include <comdef.h>

std::wstring DxException::ToString() const {
    _com_error err(hr);
    return func + L" failed in " + file + L"; line " + std::to_wstring(line) + L"; " + err.ErrorMessage();
}
