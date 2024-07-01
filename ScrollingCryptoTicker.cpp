#include <windows.h>
#include <wininet.h>
#include <string>
#include <sstream>
#include <iomanip>
#include <shlwapi.h>
#include <stdexcept> // for std::exception
#include "json.hpp"
using json = nlohmann::json;

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shlwapi.lib")

const wchar_t g_szClassName[] = L"myWindowClass";
const int TICKER_HEIGHT = 50;
const int UPDATE_INTERVAL = 60000; // 1 minute
const int SCROLL_SPEED = 2; // Pixels to move per timer tick
const int ID_EXIT = 9001; // Command ID for the "Exit" menu item

std::wstring waxpPrice = L"Fetching price... - ";
int xOffset = 0;
int textWidth = 0;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

std::wstring FetchWaxpPrice() {
    HINTERNET hInternet = InternetOpen(L"MyApp", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (!hInternet) throw std::runtime_error("Failed to open internet");

    HINTERNET hConnect = InternetOpenUrl(hInternet, L"https://api.coingecko.com/api/v3/simple/price?ids=wax&vs_currencies=usd", NULL, 0, INTERNET_FLAG_RELOAD, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        throw std::runtime_error("Failed to open connect");
    }

    char buffer[512];
    DWORD bytesRead;
    std::string response;

    while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        response.append(buffer, bytesRead);
    }

    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    // Parse the JSON response (using nlohmann/json library for robustness)
    try {
        auto json = nlohmann::json::parse(response);
        double price = json["wax"]["usd"];
        std::wstringstream ws;
        ws << std::fixed << std::setprecision(8) << price;
        return ws.str() + L" - ";
    }
    catch (const std::exception& e) {
        return L"Error - ";
    }
}

void UpdateWaxpPrice(HWND hwnd) {
    std::wstring newPrice = FetchWaxpPrice();

    if (newPrice != L"Error - ") {
        waxpPrice = newPrice;
    }
    else {
        // Handle error fetching price
        waxpPrice = L"Error fetching price - ";
    }

    HDC hdc = GetDC(hwnd);
    if (hdc == NULL) {
        MessageBox(hwnd, L"Failed to get device context!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }

    HFONT hFont = CreateFont(TICKER_HEIGHT, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    if (hFont == NULL) {
        MessageBox(hwnd, L"Failed to create font!", L"Error", MB_OK | MB_ICONERROR);
        ReleaseDC(hwnd, hdc);
        return;
    }

    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    SIZE textSize;
    if (!GetTextExtentPoint32(hdc, waxpPrice.c_str(), waxpPrice.length(), &textSize)) {
        MessageBox(hwnd, L"Failed to get text extent!", L"Error", MB_OK | MB_ICONERROR);
        SelectObject(hdc, hOldFont);
        DeleteObject(hFont);
        ReleaseDC(hwnd, hdc);
        return;
    }
    textWidth = textSize.cx;

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
    ReleaseDC(hwnd, hdc);

    InvalidateRect(hwnd, NULL, TRUE);
}

void DrawTicker(HWND hwnd, HDC hdc) {
    RECT rect;
    GetClientRect(hwnd, &rect); // Retrieve client area dimensions

    HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
    if (hBrush == NULL) {
        MessageBox(hwnd, L"Failed to create brush!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    FillRect(hdc, &rect, hBrush);
    DeleteObject(hBrush);

    SetTextColor(hdc, RGB(255, 255, 255));
    SetBkMode(hdc, TRANSPARENT);

    HFONT hFont = CreateFont(TICKER_HEIGHT, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Arial");
    if (hFont == NULL) {
        MessageBox(hwnd, L"Failed to create font!", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

    if (textWidth == 0) {
        // Handle the case where textWidth is zero
        // For example, set a default width to prevent division by zero
        textWidth = 1;
    }

    int repetitions = (rect.right / textWidth) + 2;

    // Draw the ticker text
    for (int i = 0; i < repetitions; ++i) {
        TextOut(hdc, xOffset + i * textWidth, 0, waxpPrice.c_str(), waxpPrice.length());
    }

    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

int GetTaskbarHeight() {
    APPBARDATA abd;
    abd.cbSize = sizeof(APPBARDATA);
    if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd)) {
        if (abd.rc.bottom == GetSystemMetrics(SM_CYSCREEN)) {
            return abd.rc.bottom - abd.rc.top;
        }
    }
    return 0;
}

void CreateExitMenu(HWND hwnd) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_EXIT, L"Exit");
    SetMenu(hwnd, hMenu);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc;
    HWND hwnd;
    MSG Msg;

    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = g_szClassName;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int taskbarHeight = GetTaskbarHeight();
    int tickerYPosition = screenHeight - taskbarHeight - TICKER_HEIGHT;

    hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        g_szClassName,
        NULL,
        WS_POPUP,
        0, tickerYPosition, screenWidth, TICKER_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (hwnd == NULL) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error!", MB_ICONEXCLAMATION | MB_OK);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    SetTimer(hwnd, 1, UPDATE_INTERVAL, NULL);
    SetTimer(hwnd, 2, 50, NULL); // Timer for scrolling

    UpdateWaxpPrice(hwnd);

    while (GetMessage(&Msg, NULL, 0, 0) > 0) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
    }

    return Msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_TIMER:
        if (wParam == 1) { // Update price timer
            UpdateWaxpPrice(hwnd);
        }
        else if (wParam == 2) { // Scrolling timer
            xOffset -= SCROLL_SPEED;
            if (xOffset <= -textWidth) {
                xOffset = 0;
            }
            InvalidateRect(hwnd, NULL, FALSE); // Only invalidate client area to reduce flickering
        }
        break;
    case WM_CONTEXTMENU:
    {
        HMENU hMenu = CreatePopupMenu();
        if (hMenu) {
            AppendMenu(hMenu, MF_STRING, ID_EXIT, L"Exit");
            POINT pt;
            GetCursorPos(&pt);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
    }
    break;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_EXIT:
            PostQuitMessage(0);
            break;
        }
        break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (hdc == NULL) {
            MessageBox(hwnd, L"Failed to get device context in WM_PAINT!", L"Error", MB_OK | MB_ICONERROR);
            break;
        }

        DrawTicker(hwnd, hdc);

        EndPaint(hwnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
