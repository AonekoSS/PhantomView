#pragma once
#include "InfoList.h"

class MetaExtractor {
public:
    static info_list ExtractMeta(const std::wstring& filePath);
};