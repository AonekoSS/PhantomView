#pragma once
#include "InfoList.h"
#include <vector>
#include <cstdint>

class NAIExtractor {
public:
    static info_list ExtractNAI(const std::wstring& filePath);

private:
    static void ExtractNovelAIData(const std::vector<uint8_t>& lsb_bytes, info_list& result);
    static std::pair<std::wstring, std::wstring> DecompressGzipData(const uint8_t* comp_data, uint32_t length);
};
