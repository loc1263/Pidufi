#pragma once
#include <string>

std::string ToUtf8(const std::wstring& w);
std::wstring FromUtf8(const char* s);
