#include "framework.h"
#include <sstream>

#include "TextUtils.h"

// UTF-8→ユニコード変換
std::wstring utf8_to_unicode(const std::string& utf8_string) {
	if (utf8_string.empty()) {
		return std::wstring();
	}

	int size = MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, nullptr, 0);
	if (size == 0) {
		return std::wstring();
	}

	std::vector<wchar_t> buffer(size);
	int result = MultiByteToWideChar(CP_UTF8, 0, utf8_string.c_str(), -1, buffer.data(), size);
	if (result == 0) {
		return std::wstring();
	}

	return std::wstring(buffer.begin(), buffer.end() - 1);
}

// ユニコード→UTF-8変換
std::string unicode_to_utf8(const std::wstring& unicode_string) {
	if (unicode_string.empty()) {
		return std::string();
	}
	int size = WideCharToMultiByte(CP_UTF8, 0, unicode_string.c_str(), -1, nullptr, 0, nullptr, nullptr);
	if (size == 0) {
		return std::string();
	}
	std::vector<char> buffer(size);
	int result = WideCharToMultiByte(CP_UTF8, 0, unicode_string.c_str(), -1, buffer.data(), size, nullptr, nullptr);
	if (result == 0) {
		return std::string();
	}
	return std::string(buffer.begin(), buffer.end() - 1);
}
