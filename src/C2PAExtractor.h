#pragma once
#include "InfoList.h"

class C2PAExtractor {
public:
    static info_list ExtractC2PA(const std::wstring& filePath);
};
