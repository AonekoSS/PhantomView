#include "framework.h"
#include "C2PAExtractor.h"
#include "TextUtils.h"
#include <c2pa.hpp>

info_list C2PAExtractor::ExtractC2PA(const std::wstring& imagePath) {
    info_list result;

    try {
        // ファイルパスをUTF-8に変換
        std::string utf8Path = unicode_to_utf8(imagePath);

        // C2PA情報を読み込み
        c2pa::Reader reader(utf8Path);

        // マニフェストをJSONとして取得
        std::string manifest_json = reader.json();

        if (!manifest_json.empty()) {
            result.push_back({L"C2PA_JSON", utf8_to_unicode(manifest_json)});
        }
	} catch (...) {
	}
    return result;
}
