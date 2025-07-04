#include "framework.h"
#include <Richedit.h>
#include <shellapi.h>
#include <sstream>

#include "PhantomView.h"
#include "MetaExtractor.h"
#include "C2PAExtractor.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    PhantomView app;
    if (!app.Initialize(hInstance)) return -1;
    return app.Run();
}

bool PhantomView::Initialize(HINSTANCE hInstance) {
    LoadLibrary(TEXT("Msftedit.dll"));

    const auto szWindowClass = L"PhantomViewWindowClass";
    WNDCLASSEXW wcex{
        .cbSize = sizeof(WNDCLASSEXW),
        .style = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc = PhantomView::WindowProc,
        .cbClsExtra = 0,
        .cbWndExtra = sizeof(void*),
        .hInstance = hInstance,
        .hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PHANTOMVIEW)),
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = (HBRUSH)(COLOR_WINDOW + 1),
        .lpszMenuName = NULL,
        .lpszClassName = szWindowClass,
        .hIconSm = NULL
    };
    RegisterClassExW(&wcex);

    m_hwnd = CreateWindowExW(
        0,
        szWindowClass,
        L"PhantomView",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hwnd) {
        return false;
    }
    DragAcceptFiles(m_hwnd, TRUE);
    ShowWindow(m_hwnd, SW_SHOW);
    UpdateWindow(m_hwnd);
    return true;
}

int PhantomView::Run() {
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK PhantomView::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PhantomView* pThis = nullptr;
    if (message == WM_NCCREATE) {
        CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
        pThis = (PhantomView*)pcs->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (PhantomView*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }

    if (!pThis) {
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }

    switch (message) {
    case WM_CREATE:
        pThis->OnCreate(hWnd);
        return 0;
    case WM_SIZE:
        pThis->OnSize(hWnd);
        return 0;
    case WM_DROPFILES:
        pThis->OnDropFiles(hWnd, wParam);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, message, wParam, lParam);
    }
}

void PhantomView::OnCreate(HWND hwnd) {
    RECT rc;
    GetClientRect(hwnd, &rc);
    m_hbox = CreateWindowExW(0, MSFTEDIT_CLASS, NULL,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, rc.right, rc.bottom,
        hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    // 背景色を黒に設定
    SendMessageW(m_hbox, EM_SETBKGNDCOLOR, 0, RGB(0,0,0));
}

void PhantomView::OnSize(HWND hwnd) {
    if (m_hbox) {
        RECT rc;
        GetClientRect(hwnd, &rc);
        MoveWindow(m_hbox, 0, 0, rc.right, rc.bottom, TRUE);
    }
}

void PhantomView::OnDropFiles(HWND hwnd, WPARAM wParam) {
    HDROP hDrop = (HDROP)wParam;
    WCHAR szFile[MAX_PATH];
    if (DragQueryFileW(hDrop, 0, szFile, MAX_PATH)) {
        SendMessageW(m_hbox, EM_SETSEL, -1, -1);
        SendMessageW(m_hbox, EM_REPLACESEL, FALSE, (LPARAM)szFile);
        SendMessageW(m_hbox, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
        InspectImage(szFile);
    }
    DragFinish(hDrop);
}

bool PhantomView::IsJson(const std::wstring& text) {
    // 先頭と末尾の文字だけで簡易判定
    size_t start = text.find_first_not_of(L" \t\r\n");
    size_t end = text.find_last_not_of(L" \t\r\n");
    if (start == std::wstring::npos || end == std::wstring::npos) return false;
    wchar_t first = text[start];
    wchar_t last = text[end];
    return (first == L'{' && last == L'}') || (first == L'[' && last == L']');
}

bool PhantomView::IsXml(const std::wstring& text) {
    // 先頭と末尾の文字だけで簡易判定
    size_t start = text.find_first_not_of(L" \t\r\n");
    size_t end = text.find_last_not_of(L" \t\r\n");
    if (start == std::wstring::npos || end == std::wstring::npos) return false;
    wchar_t first = text[start];
    wchar_t last = text[end];
    return (first == L'<' && last == L'>');
}

// セクションの出力
void PhantomView::OutputSection(const std::wstring& title, const info_list& data) {
	if (data.empty()) return;

    SetColor(COLOR_TITLE);
    PutText(title + L"\r\n");
    for (const auto& kv : data) {
		auto& key = kv.first;
		auto& val = kv.second;
        SetColor(COLOR_KEY);
        PutText(key + L" : ");
        if (IsJson(val)) {
            SetColor(COLOR_VALUE);
            PutText(L"\r\n");
            OutputAsJSON(val);
            PutText(L"\r\n");
        } else if (IsXml(val)) {
            SetColor(COLOR_VALUE);
            PutText(L"\r\n");
            OutputAsXML(val);
            PutText(L"\r\n");
        } else {
            SetColor(COLOR_VALUE);
            PutText(val + L"\r\n");
        }
    }
}

bool PhantomView::InspectImage(const std::wstring& path) {
    SendMessageW(m_hbox, WM_SETTEXT, 0, (LPARAM)L"");

	// メタデータ抽出
    auto meta = MetaExtractor::ExtractMeta(path);
    OutputSection(L"[MetaData]", meta);

	// C2PA抽出
    auto c2pa = C2PAExtractor::ExtractC2PA(path);
    OutputSection(L"[C2PA]", c2pa);

    return true;
}

// 追加: 色付きテキスト挿入用のヘルパー関数
void PhantomView::SetColor(COLORREF color) {
    CHARRANGE cr;
    SendMessageW(m_hbox, EM_EXGETSEL, 0, (LPARAM)&cr);
    CHARFORMAT2 cf = {0};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(m_hbox, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
}
void PhantomView::PutText(const std::wstring& text) {
    SendMessageW(m_hbox, EM_SETSEL, -1, -1);
    SendMessageW(m_hbox, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
}

// JSON整形＆色分け出力
void PhantomView::OutputAsJSON(const std::wstring& json) {
    int indent = 0;
    bool inString = false;
    bool isKey = true;
    std::wstring keyBuffer, valBuffer;
    for (size_t i = 0; i < json.size(); ++i) {
        wchar_t c = json[i];
        // 空配列・空オブジェクトの特別処理
        if (!inString && (c == L'{' || c == L'[')) {
            wchar_t close = (c == L'{') ? L'}' : L']';
            size_t j = i + 1;
            while (j < json.size() && iswspace(json[j])) ++j;
            if (j < json.size() && json[j] == close) {
                SetColor(COLOR_SYMBOL);
                PutText(std::wstring(1, c) + std::wstring(1, close));
                i = j;
                continue;
            }
        }
        if (c == L'"') {
            inString = !inString;
            if (inString) {
                // 文字列開始
                if (isKey) SetColor(COLOR_ATTR);
                else SetColor(COLOR_VALUE);
                PutText(L"\"");
            } else {
                PutText(L"\"");
                SetColor(COLOR_SYMBOL);
            }
            continue;
        }
        if (!inString) {
            if (c == L'{' || c == L'[') {
                SetColor(COLOR_SYMBOL);
                PutText(std::wstring(1, c) + L"\r\n");
                indent++;
                PutText(std::wstring(indent * INDENT_WIDTH, L' '));
                continue;
            }
            if (c == L'}' || c == L']') {
                SetColor(COLOR_SYMBOL);
                PutText(L"\r\n" + std::wstring((indent-1) * INDENT_WIDTH, L' ') + std::wstring(1, c));
                indent--;
                continue;
            }
            if (c == L',') {
                SetColor(COLOR_SYMBOL);
                PutText(L",");
                PutText(L"\r\n" + std::wstring(indent * INDENT_WIDTH, L' '));
                isKey = true;
                continue;
            }
            if (c == L':') {
                SetColor(COLOR_SYMBOL);
                PutText(L": ");
                isKey = false;
                continue;
            }
            if (iswspace(c)) continue;
        }
        // 文字列本体
        if (inString) {
            PutText(std::wstring(1, c));
        } else if (!iswspace(c)) {
            // 数値やtrue/false/null
            SetColor(COLOR_VALUE);
            PutText(std::wstring(1, c));
        }
    }
    SetColor(COLOR_VALUE);
}

// XML整形＆色分け出力
void PhantomView::OutputAsXML(const std::wstring& xml) {
    int indent = 0;
    bool inTag = false;
    bool inAttr = false;
    bool inValue = false;
    std::wstring buffer;
    bool firstTag = true;
    for (size_t i = 0; i < xml.size(); ++i) {
        wchar_t c = xml[i];
        if (c == L'<') {
            if (!buffer.empty()) {
                SetColor(COLOR_VALUE);
                PutText(buffer);
                buffer.clear();
            }
            SetColor(COLOR_SYMBOL);
            if (!firstTag) {
                PutText(L"\r\n" + std::wstring(indent * INDENT_WIDTH, L' '));
            }
            PutText(L"<");
            inTag = true;
            inAttr = false;
            inValue = false;
            firstTag = false;
            continue;
        }
        if (c == L'>') {
            if (!buffer.empty()) {
                SetColor(COLOR_TAG);
                PutText(buffer);
                buffer.clear();
            }
            SetColor(COLOR_SYMBOL);
            PutText(L">");
            inTag = false;
            inValue = true;
            if (i > 0 && xml[i-1] == L'/') {
                // 空要素タグ
                inValue = false;
            }
            continue;
        }
        if (inTag) {
            if (c == L' ') {
                if (!buffer.empty()) {
                    SetColor(COLOR_TAG);
                    PutText(buffer);
                    buffer.clear();
                }
                SetColor(COLOR_SYMBOL);
                PutText(L" ");
                inAttr = true;
                continue;
            }
            if (c == L'=') {
                if (!buffer.empty()) {
                    SetColor(COLOR_ATTR);
                    PutText(buffer);
                    buffer.clear();
                }
                SetColor(COLOR_SYMBOL);
                PutText(L"=");
                continue;
            }
            if (c == L'"') {
                SetColor(COLOR_VALUE);
                PutText(L"\"");
                size_t j = i+1;
                std::wstring val;
                while (j < xml.size() && xml[j] != L'"') val += xml[j++];
                PutText(val);
                PutText(L"\"");
                i = j;
                continue;
            }
            buffer += c;
        } else if (inValue) {
            if (c == L'<') {
                inValue = false;
                --i;
                continue;
            }
            buffer += c;
        }
    }
    if (!buffer.empty()) {
        SetColor(COLOR_VALUE);
        PutText(buffer);
    }
    SetColor(COLOR_VALUE);
}
