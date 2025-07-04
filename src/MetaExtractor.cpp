#include "framework.h"
#include "MetaExtractor.h"
#include "TextUtils.h"
#include <fstream>
#include <vector>
#include <algorithm>
#include <map>

// EXIFタグの定義
enum ExifTag {
	TAG_MAKE = 0x010F,
	TAG_MODEL = 0x0110,
	TAG_DATETIME = 0x0132,
	TAG_EXPOSURETIME = 0x829A,
	TAG_FNUMBER = 0x829D,
	TAG_ISOSPEEDRATINGS = 0x8827,
	TAG_FOCALLENGTH = 0x920A,
	TAG_ARTIST = 0x013B,
	TAG_COPYRIGHT = 0x8298,
	TAG_SOFTWARE = 0x0131,
	TAG_USERCOMMENT = 0x9286
};

// EXIFデータ型の定義
enum ExifDataType {
	TYPE_BYTE = 1,
	TYPE_ASCII = 2,
	TYPE_SHORT = 3,
	TYPE_LONG = 4,
	TYPE_RATIONAL = 5,
	TYPE_UNDEFINED = 7,
	TYPE_SLONG = 9,
	TYPE_SRATIONAL = 10
};

// バイトオーダーを判定する関数
static bool IsLittleEndian(const std::vector<char>& data, size_t offset) {
	if (offset + 2 > data.size()) return false;
	return (data[offset] == 'I' && data[offset + 1] == 'I');
}

// 16ビット値を読み込む関数
static uint16_t ReadUInt16(const std::vector<char>& data, size_t offset, bool littleEndian) {
	if (offset + 2 > data.size()) return 0;
	uint16_t value;
	memcpy(&value, &data[offset], 2);
	return littleEndian ? value : _byteswap_ushort(value);
}

// 32ビット値を読み込む関数
static uint32_t ReadUInt32(const std::vector<char>& data, size_t offset, bool littleEndian) {
	if (offset + 4 > data.size()) return 0;
	uint32_t value;
	memcpy(&value, &data[offset], 4);
	return littleEndian ? value : _byteswap_ulong(value);
}

// 有理数を読み込む関数
static std::wstring ReadRational(const std::vector<char>& data, size_t offset, bool littleEndian) {
	if (offset + 8 > data.size()) return L"";
	uint32_t numerator = ReadUInt32(data, offset, littleEndian);
	uint32_t denominator = ReadUInt32(data, offset + 4, littleEndian);

	if (denominator == 0) return L"";

	double value = static_cast<double>(numerator) / denominator;
	wchar_t buffer[64];
	swprintf_s(buffer, L"%.2f", value);
	return std::wstring(buffer);
}

// ASCII文字列を読み込む関数
static std::wstring ReadASCII(const std::vector<char>& data, size_t offset, size_t count) {
	if (offset + count > data.size()) return L"";
	std::string str(data.begin() + offset, data.begin() + offset + count);
	// NULL文字を除去
	if (!str.empty() && str.back() == '\0') {
		str.pop_back();
	}
	return utf8_to_unicode(str);
}

// EXIFタグ名を取得する関数
static std::wstring GetTagName(uint16_t tag) {
	static const std::map<uint16_t, std::wstring> tagNames = {
		{TAG_MAKE, L"メーカー"},
		{TAG_MODEL, L"モデル"},
		{TAG_DATETIME, L"撮影日時"},
		{TAG_EXPOSURETIME, L"露出時間"},
		{TAG_FNUMBER, L"F値"},
		{TAG_ISOSPEEDRATINGS, L"ISO感度"},
		{TAG_FOCALLENGTH, L"焦点距離"},
		{TAG_ARTIST, L"アーティスト"},
		{TAG_COPYRIGHT, L"著作権"},
		{TAG_SOFTWARE, L"ソフトウェア"},
		{TAG_USERCOMMENT, L"ユーザーコメント"}
	};

	auto it = tagNames.find(tag);
	return it != tagNames.end() ? it->second : L"タグ" + std::to_wstring(tag);
}

// EXIFデータを解析する関数
static std::wstring ParseExifValue(const std::vector<char>& data, size_t offset, uint16_t dataType, uint32_t count, bool littleEndian) {
	switch (dataType) {
		case TYPE_BYTE:
			if (count == 1 && offset < data.size()) {
				return std::to_wstring(static_cast<uint8_t>(data[offset]));
			}
			break;

		case TYPE_ASCII:
			return ReadASCII(data, offset, count);

		case TYPE_SHORT:
			if (count == 1) {
				return std::to_wstring(ReadUInt16(data, offset, littleEndian));
			}
			break;

		case TYPE_LONG:
			if (count == 1) {
				return std::to_wstring(ReadUInt32(data, offset, littleEndian));
			}
			break;

		case TYPE_RATIONAL:
			if (count == 1) {
				return ReadRational(data, offset, littleEndian);
			}
			break;

		case TYPE_UNDEFINED:
			if (count <= 4) {
				std::wstring result = L"";
				for (uint32_t i = 0; i < count && offset + i < data.size(); ++i) {
					if (i > 0) result += L" ";
					result += std::to_wstring(static_cast<uint8_t>(data[offset + i]));
				}
				return result;
			}
			break;
	}

	return L"";
}

// EXIFチャンクを解析する関数
static info_list ReadExifChunk(std::ifstream& file, size_t chunk_size) {
	info_list list;

	// チャンクデータを読み込む
	std::vector<char> data(chunk_size);
	file.read(data.data(), chunk_size);

	if (data.size() < 8) return list;

	// バイトオーダーを判定
	bool littleEndian = IsLittleEndian(data, 0);

	// TIFFマジックナンバーを確認
	uint16_t magic = ReadUInt16(data, 2, littleEndian);
	if (magic != 0x2A) return list;

	// 最初のIFDオフセットを読み込む
	uint32_t ifdOffset = ReadUInt32(data, 4, littleEndian);

	// IFDを解析
	while (ifdOffset > 0 && ifdOffset + 2 < data.size()) {
		// IFDエントリ数を読み込む
		uint16_t entryCount = ReadUInt16(data, ifdOffset, littleEndian);

		if (entryCount > 1000) break; // 異常値の場合は終了

		// 各エントリを解析
		for (uint16_t i = 0; i < entryCount; ++i) {
			size_t entryOffset = ifdOffset + 2 + i * 12;
			if (entryOffset + 12 > data.size()) break;

			// タグ、データ型、カウント、値を読み込む
			uint16_t tag = ReadUInt16(data, entryOffset, littleEndian);
			uint16_t dataType = ReadUInt16(data, entryOffset + 2, littleEndian);
			uint32_t count = ReadUInt32(data, entryOffset + 4, littleEndian);
			uint32_t value = ReadUInt32(data, entryOffset + 8, littleEndian);

			// データサイズを計算
			size_t dataSize = 0;
			switch (dataType) {
				case TYPE_BYTE: dataSize = 1; break;
				case TYPE_ASCII: dataSize = 1; break;
				case TYPE_SHORT: dataSize = 2; break;
				case TYPE_LONG: dataSize = 4; break;
				case TYPE_RATIONAL: dataSize = 8; break;
				case TYPE_UNDEFINED: dataSize = 1; break;
				default: continue;
			}

			std::wstring tagValue;
			if (count * dataSize <= 4) {
				// 値が4バイト以内の場合は直接値として扱う
				tagValue = ParseExifValue(data, entryOffset + 8, dataType, count, littleEndian);
			} else {
				// 値が4バイトを超える場合はオフセットとして扱う
				if (value < data.size()) {
					tagValue = ParseExifValue(data, value, dataType, count, littleEndian);
				}
			}

			if (!tagValue.empty()) {
				list.push_back(std::make_pair(GetTagName(tag), tagValue));
			}
		}

		// 次のIFDオフセットを読み込む
		size_t nextIfdOffset = ifdOffset + 2 + entryCount * 12;
		if (nextIfdOffset + 4 <= data.size()) {
			ifdOffset = ReadUInt32(data, nextIfdOffset, littleEndian);
		} else {
			break;
		}
	}

	return list;
}

static info_list ExtractFromPNG(const std::wstring& filePath) {
	info_list list;
	std::ifstream file(filePath, std::ios::binary);

	if (!file) {
		return list;
	}

	// PNGシグネチャの確認
	uint8_t signature[8];
	file.read(reinterpret_cast<char*>(signature), 8);
	if (memcmp(signature, "\x89PNG\r\n\x1a\n", 8) != 0) {
		return list;
	}

	// チャンクの読み込み
	while (file) {
		// チャンクの長さを読み込む
		uint32_t chunk_length;
		file.read(reinterpret_cast<char*>(&chunk_length), 4);
		chunk_length = _byteswap_ulong(chunk_length);  // ビッグエンディアンから変換

		// チャンクタイプを読み込む
		char chunk_type[4];
		file.read(chunk_type, 4);

		// IENDチャンクが見つかったら終了
		if (memcmp(chunk_type, "IEND", 4) == 0) {
			break;
		}

		if (memcmp(chunk_type, "tEXt", 4) != 0) {
			// tEXtチャンク以外はスキップ
			file.seekg(chunk_length + 4, std::ios::cur);
			continue;
		}

		// チャンクデータを読み込む
		std::vector<uint8_t> chunk;
		chunk.resize(chunk_length);
		file.read(reinterpret_cast<char*>(chunk.data()), chunk_length);

		// CRCをスキップ
		file.seekg(4, std::ios::cur);

		std::string data(reinterpret_cast<const char*>(chunk.data()), chunk.size());

		// tEXtチャンクは "keyword\0text" 形式なので、最初のNULL文字で分割
		auto null_pos = data.find('\0');
		if (null_pos == std::string::npos) continue;
		auto keyword = data.substr(0, null_pos);
		auto text = data.substr(null_pos + 1);

		list.push_back(std::make_pair(utf8_to_unicode(keyword), utf8_to_unicode(text)));
	}
	file.close();
	return list;
}



static info_list ExtractFromJPEG(const std::wstring& filePath) {
	info_list list;

	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		return list;
	}

	// JPEGファイルの先頭を確認
	uint8_t header[2];
	file.read(reinterpret_cast<char*>(header), 2);
	if (header[0] != 0xFF || header[1] != 0xD8) {
		return list;  // JPEGファイルではない
	}

	while (file) {
		uint8_t marker[2];
		file.read(reinterpret_cast<char*>(marker), 2);

		// マーカーの確認
		if (marker[0] != 0xFF) {
			break;
		}

		// APP1マーカー（Exif）を探す
		if (marker[1] == 0xE1) {
			// セグメントサイズを読み込む
			uint16_t size;
			file.read(reinterpret_cast<char*>(&size), 2);
			size = _byteswap_ushort(size);  // ビッグエンディアンから変換

			// Exifヘッダーを確認
			char exif_header[6];
			file.read(exif_header, 6);
			if (memcmp(exif_header, "Exif\0\0", 6) == 0) {
				auto exifInfo = ReadExifChunk(file, size - 8);
				list.insert(list.end(), exifInfo.begin(), exifInfo.end());
			}
		}
		else if (marker[1] == 0xDA) {  // SOSマーカー（画像データの開始）
			break;
		}
		else {
			// その他のマーカーはスキップ
			uint16_t size;
			file.read(reinterpret_cast<char*>(&size), 2);
			size = _byteswap_ushort(size);
			file.seekg(size - 2, std::ios::cur);
		}
	}

	return list;
}

// Webp画像のプロンプト抽出
static info_list ExtractFromWEBP(const std::wstring& filePath) {
	info_list list;

	std::ifstream file(filePath, std::ios::binary);
	if (!file) {
		return list;
	}

	// WebPファイルの先頭を確認
	char header[4];
	file.read(header, 4);
	if (memcmp(header, "RIFF", 4) != 0) {
		return list;  // WebPファイルではない
	}

	// ファイルサイズを読み込む
	uint32_t fileSize;
	file.read(reinterpret_cast<char*>(&fileSize), 4);
	fileSize = _byteswap_ulong(fileSize);  // リトルエンディアンから変換

	// WebPシグネチャを確認
	char webp_header[4];
	file.read(webp_header, 4);
	if (memcmp(webp_header, "WEBP", 4) != 0) {
		return list;
	}

	// チャンクを探す
	while (file) {
		char chunk_header[5]; chunk_header[4] = '\0';
		file.read(chunk_header, 4);
		if (file.eof()) break;

		// チャンクサイズを読み込む
		uint32_t chunk_size;
		file.read(reinterpret_cast<char*>(&chunk_size), 4);

		// EXIFチャンクを探す
		if (memcmp(chunk_header, "EXIF", 4) == 0) {
			auto exifInfo = ReadExifChunk(file, chunk_size);
			list.insert(list.end(), exifInfo.begin(), exifInfo.end());
		}
		else {
			// その他のチャンクはスキップ
			file.seekg(chunk_size, std::ios::cur);
		}
	}

	return list;
}

// ファイル情報の読み込み
info_list MetaExtractor::ExtractMeta(const std::wstring& filePath) {
	std::wstring ext = filePath.substr(filePath.find_last_of(L".") + 1);
	std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

	if (ext == L"png") {
		auto info = ExtractFromPNG(filePath);
		return info;
	}
	if (ext == L"jpg" || ext == L"jpeg") {
		auto info = ExtractFromJPEG(filePath);
		return info;
	}
	if (ext == L"webp") {
		auto info = ExtractFromWEBP(filePath);
		return info;
	}

	return {};
}


