#pragma once

#include <wx/string.h>

inline wxString WxUtf8(const char* text)
{
    return wxString::FromUTF8(text);
}

#define WXU8(x) WxUtf8(x)