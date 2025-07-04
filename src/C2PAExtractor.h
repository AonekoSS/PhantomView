#pragma once
#include "InfoList.h"
#include <string>

class C2PAExtractor {
public:
    static info_list ExtractC2PA(const std::wstring& filePath);
};
