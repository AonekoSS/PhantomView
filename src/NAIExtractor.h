#pragma once
#include "InfoList.h"
#include <string>

class NAIExtractor {
public:
    static info_list ExtractNAI(const std::wstring& filePath);
};
