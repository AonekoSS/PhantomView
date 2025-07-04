#pragma once
#include "InfoList.h"

class NAIExtractor {
public:
    static info_list ExtractNAI(const std::wstring& filePath);
};
