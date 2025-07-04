#pragma once

#include <string>
#include <vector>

// UTF-8→ユニコード変換
std::wstring utf8_to_unicode(const std::string& utf8_string);

// ユニコード→UTF-8変換
std::string unicode_to_utf8(const std::wstring& unicode_string);
