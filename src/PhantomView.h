#pragma once
#include <string>
#include <map>

#include "resource.h"
#include "InfoList.h"

class PhantomView {
public:
	bool Initialize(HINSTANCE hInstance);
	int Run();
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void OnCreate(HWND hwnd);
	void OnSize(HWND hwnd);
	void OnDropFiles(HWND hwnd, WPARAM wParam);
	bool InspectImage(const std::wstring& path);
	void SetColor(COLORREF color);
	void PutText(const std::wstring& text);
	bool IsJson(const std::wstring& text);
	void OutputAsJSON(const std::wstring& json);
	bool IsXml(const std::wstring& text);
	void OutputAsXML(const std::wstring& xml);
	void OutputSection(const std::wstring& title, const info_list& data);

	static constexpr COLORREF COLOR_KEY    = RGB(0,255,255); // シアン
	static constexpr COLORREF COLOR_VALUE  = RGB(255,255,255); // 白
	static constexpr COLORREF COLOR_TAG    = RGB(0,255,255); // シアン
	static constexpr COLORREF COLOR_ATTR   = RGB(255,255,0); // 黄色
	static constexpr COLORREF COLOR_SYMBOL = RGB(192,192,192); // グレー
	static constexpr COLORREF COLOR_TITLE  = RGB(0,255,0); // 緑
	static constexpr COLORREF COLOR_BG     = RGB(0,0,0); // 黒
	static constexpr int INDENT_WIDTH = 4;

private:
	HWND m_hwnd = nullptr;
	HWND m_hbox = nullptr;
};
