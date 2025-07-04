#include "framework.h"
#include <zlib.h>

#include "NAIExtractor.h"
#include "TextUtils.h"
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <algorithm>

info_list NAIExtractor::ExtractNAI(const std::wstring& imagePath) {
    using namespace Gdiplus;
    info_list result;

    // GDI+を使用
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    Gdiplus::Bitmap* bitmap = nullptr;

    try {
        bitmap = Gdiplus::Bitmap::FromFile(imagePath.c_str());
        if (!bitmap) {
            Gdiplus::GdiplusShutdown(gdiplusToken);
            return result;
        }

		// ピクセルデータを取得
		Gdiplus::BitmapData bitmapData;
		Rect rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight());
        Gdiplus::Status status = bitmap->LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &bitmapData);
        if (status != Gdiplus::Ok) {
            throw std::runtime_error("LockBits失敗");
		}

        // アルファチャンネル抽出
        std::vector<unsigned char> alpha;
        alpha.reserve(rect.Width * rect.Height);
        unsigned char* p = static_cast<unsigned char*>(bitmapData.Scan0);
		for (int x = 0; x < rect.Width; ++x) {
            for (int y = 0; y < rect.Height; ++y) {
				unsigned char* row = p + y * bitmapData.Stride;
                unsigned char a = row[x * 4 + 3]; // BGRA順
                alpha.push_back(a);
            }
        }
        bitmap->UnlockBits(&bitmapData);

        // LSBビットストリームを作成
        std::vector<uint8_t> lsb_bits;
        lsb_bits.reserve(alpha.size());
        for (unsigned char a : alpha) {
            lsb_bits.push_back(a & 0x01);
        }
        // 8ビットごとにまとめてバイト列化
        std::vector<uint8_t> lsb_bytes;
        uint8_t acc = 0;
        int bit_count = 0;
        for (size_t i = 0; i < lsb_bits.size(); ++i) {
            acc = (acc << 1) | lsb_bits[i];
            bit_count++;
            if (bit_count == 8) {
                lsb_bytes.push_back(acc);
                acc = 0;
                bit_count = 0;
            }
        }
        if (bit_count > 0) {
            acc <<= (8 - bit_count);
            lsb_bytes.push_back(acc);
        }

        // lsb_bytesからNovelAI仕様でJSONを抽出
        if (lsb_bytes.size() > 19) {
            const char nai_magic[] = "stealth_pngcomp";
            if (memcmp(lsb_bytes.data(), nai_magic, 15) == 0) {
                // データ長
                uint32_t datalen = (lsb_bytes[15] << 24) | (lsb_bytes[16] << 16) | (lsb_bytes[17] << 8) | lsb_bytes[18];
                if (lsb_bytes.size() >= 19 + datalen) {
                    const uint8_t* comp_data = lsb_bytes.data() + 19;
                    // gzip展開処理
                    z_stream strm = {};
                    strm.next_in = (Bytef*)comp_data;
                    strm.avail_in = datalen;
                    std::vector<uint8_t> jsonbuf(datalen * 8);
                    strm.next_out = jsonbuf.data();
                    strm.avail_out = static_cast<uInt>(jsonbuf.size());

                    int ret = inflateInit2(&strm, 16 + MAX_WBITS); // gzip対応
                    if (ret == Z_OK) {
                        ret = inflate(&strm, Z_FINISH);
                        if (ret == Z_STREAM_END) {
                            std::string json_str((char*)jsonbuf.data(), strm.total_out);
                            std::wstring wjson = utf8_to_unicode(json_str);
                            result.push_back({L"stealth_pngcomp", wjson});
                        } else {
                            result.push_back({L"stealth_pngcomp", L"(gzip展開失敗)"});
                        }
                        inflateEnd(&strm);
                    } else {
                        result.push_back({L"stealth_pngcomp", L"(inflateInit2失敗)"});
                    }
                } else {
                    result.push_back({L"stealth_pngcomp", L"(データ長不足)"});
                }
            } else {
                result.push_back({L"stealth_pngcomp", L"(マジック不一致)"});
            }
        } else {
            result.push_back({L"stealth_pngcomp", L"(データ長不足)"});
        }

	} catch (const std::exception& e) {
        OutputDebugStringA(e.what());
    }

	if (bitmap) delete bitmap;
    GdiplusShutdown(gdiplusToken);
    return result;
}
