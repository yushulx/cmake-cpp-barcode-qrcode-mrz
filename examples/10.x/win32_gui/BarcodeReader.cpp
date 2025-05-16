// BarcodeReader.cpp : Defines the entry point for the application.
//

#include <windows.h>
#include "framework.h"
#include "BarcodeReader.h"
#include "../../../dcv/include/DynamsoftCaptureVisionRouter.h"
#include <commdlg.h>
#include <gdiplus.h>
#include <string>
#include <vector>

#pragma comment(lib, "Gdiplus.lib")
#pragma comment(lib, "../../../dcv/lib/win/DynamsoftCorex64.lib")
#pragma comment(lib, "../../../dcv/lib/win/DynamsoftLicensex64.lib")
#pragma comment(lib, "../../../dcv/lib/win/DynamsoftCaptureVisionRouterx64.lib")
#pragma comment(lib, "../../../dcv/lib/win/DynamsoftUtilityx64.lib")

#define MAX_LOADSTRING 100

using namespace std;
using namespace Gdiplus;
using namespace dynamsoft::license;
using namespace dynamsoft::cvr;
using namespace dynamsoft::dbr;

// Global Variables:
HINSTANCE hInst;                     // current instance
WCHAR szTitle[MAX_LOADSTRING];       // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

Gdiplus::Image *g_pImage = nullptr;
int width = 0, height = 0;
HWND hwndImagePanel = nullptr, hwndResultPanel = nullptr, hwndEdit = nullptr;
CCaptureVisionRouter *cvr = nullptr;
vector<CCapturedResult *> results;

const char *ConvertWCharToChar(const WCHAR *wcharStr)
{
    // Check for nullptr input
    if (!wcharStr)
        return nullptr;

    // Calculate required buffer size
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wcharStr, -1, nullptr, 0, nullptr, nullptr);

    // Allocate buffer for UTF-8 string
    char *charStr = new char[bufferSize];

    // Perform the conversion
    WideCharToMultiByte(CP_UTF8, 0, wcharStr, -1, charStr, bufferSize, nullptr, nullptr);

    return charStr;
}

wchar_t *ConvertCharToWChar(const char *charStr)
{
    // Check for nullptr input
    if (!charStr)
        return nullptr;

    // Calculate the required buffer size
    int bufferSize = MultiByteToWideChar(CP_UTF8, 0, charStr, -1, nullptr, 0);

    // Allocate buffer for the wide character string
    wchar_t *wcharStr = new wchar_t[bufferSize];

    // Perform the conversion
    MultiByteToWideChar(CP_UTF8, 0, charStr, -1, wcharStr, bufferSize);

    return wcharStr;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Initialize GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Initialize DBR
    char errorMsgBuffer[512];
    const char *pLicense = "DLS2eyJoYW5kc2hha2VDb2RlIjoiMjAwMDAxLTE2NDk4Mjk3OTI2MzUiLCJvcmdhbml6YXRpb25JRCI6IjIwMDAwMSIsInNlc3Npb25QYXNzd29yZCI6IndTcGR6Vm05WDJrcEQ5YUoifQ==";
    CLicenseManager::InitLicense(pLicense, errorMsgBuffer, 512);
    cvr = new CCaptureVisionRouter;

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_BARCODEREADER, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_BARCODEREADER));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    GdiplusShutdown(gdiplusToken);
    return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_BARCODEREADER));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_BARCODEREADER);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        hwndImagePanel = CreateWindowEx(
            0,
            L"STATIC",
            nullptr,
            WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
            10, 10, 100, 100,
            hWnd,
            (HMENU)IDC_IMAGE_PANEL,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            nullptr);

        HWND hwndButton = CreateWindow(
            L"BUTTON",
            L"Load Image",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            10,
            10,
            100,
            30,
            hWnd,
            (HMENU)IDC_LOADIMAGE_BUTTON,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            nullptr);

        hwndResultPanel = CreateWindowEx(
            0,
            L"STATIC",
            nullptr,
            WS_CHILD | WS_VISIBLE | SS_ETCHEDFRAME,
            10, 10, 100, 100,
            hWnd,
            (HMENU)IDC_RESULT_PANEL,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            nullptr);

        HWND hwndLabel = CreateWindowEx(
            0,
            L"STATIC",
            L"Result:",
            WS_CHILD | WS_VISIBLE | SS_LEFT | SS_SIMPLE,
            10, 10, 100, 20,
            hwndResultPanel,
            (HMENU)IDC_LABEL,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            nullptr);

        hwndEdit = CreateWindowEx(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL |
                ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL,
            10, 40, 400, 200,
            hwndResultPanel,
            (HMENU)IDC_TEXTBOX,
            (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
            nullptr);

        if (!hwndEdit)
        {
            MessageBox(hWnd, L"Could not create edit box.", L"Error", MB_OK | MB_ICONERROR);
        }
    }
    break;
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        case IDC_LOADIMAGE_BUTTON:
        {
            OPENFILENAME ofn;
            TCHAR szFile[260] = {0};

            // Initialize OPENFILENAME structure
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = hWnd;
            ofn.lpstrFile = szFile;
            ofn.nMaxFile = sizeof(szFile);
            ofn.lpstrFilter = L"All\0*.*\0JPEG\0*.jpg\0PNG\0*.png\0";
            ofn.nFilterIndex = 1;
            ofn.lpstrFileTitle = nullptr;
            ofn.nMaxFileTitle = 0;
            ofn.lpstrInitialDir = nullptr;
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

            // Open the file dialog
            if (GetOpenFileName(&ofn) == TRUE)
            {
                delete g_pImage;
                g_pImage = new Gdiplus::Image(ofn.lpstrFile);

                // Decode barcodes from the image
                const char *filename = ConvertWCharToChar(ofn.lpstrFile);
                CCapturedResult *result = cvr->Capture(filename);
                results.push_back(result);
                delete filename;
                if (result->GetErrorCode() != 0)
                {
                    printf("%d, %s\n", result->GetErrorCode(), result->GetErrorString());
                }
                int capturedResultItemCount = result->GetItemsCount();

                string content = "No barcode found";
                if (capturedResultItemCount > 0)
                {
                    content = "Total barcode count: " + std::to_string(capturedResultItemCount) + "\r\n\r\n";

                    for (int j = 0; j < capturedResultItemCount; j++)
                    {
                        const CCapturedResultItem *capturedResultItem = result->GetItem(j);
                        CapturedResultItemType type = capturedResultItem->GetType();
                        if (type & CapturedResultItemType::CRIT_BARCODE)
                        {
                            const CBarcodeResultItem *barcodeResultItem = dynamic_cast<const CBarcodeResultItem *>(capturedResultItem);
                            content += "Result " + std::to_string(j + 1) + ": \r\n";
                            content += "Barcode Format: " + string(barcodeResultItem->GetFormatString()) + "\r\n";
                            content += "Barcode Text: " + string(barcodeResultItem->GetText()) + "\r\n\r\n";
                        }
                    }
                }

                wchar_t *newText = ConvertCharToWChar(content.c_str());
                SetDlgItemText(hwndResultPanel, IDC_TEXTBOX, newText);
                delete newText;

                InvalidateRect(hWnd, nullptr, TRUE);
            }
        }

        break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (g_pImage && !g_pImage->GetLastStatus())
        {
            // Set your desired width and height
            const int desiredWidth = 800;
            const int desiredHeight = 600;

            // Calculate aspect ratio and scaling factor
            float scaleX = static_cast<float>(desiredWidth) / g_pImage->GetWidth();
            float scaleY = static_cast<float>(desiredHeight) / g_pImage->GetHeight();
            float scale = min(scaleX, scaleY);

            // Calculate new dimensions
            int imageWidth = static_cast<int>(scale * g_pImage->GetWidth());
            int imageHeight = static_cast<int>(scale * g_pImage->GetHeight());

            Gdiplus::Graphics graphics(hdc);
            graphics.DrawImage(g_pImage, 10, 50, imageWidth, imageHeight);

            // Draw overlay
            if (!results.empty())
            {
                CCapturedResult *result = results[0];

                Gdiplus::Pen pen(Gdiplus::Color(255, 255, 0, 255));

                Gdiplus::FontFamily fontFamily(L"Arial");
                Gdiplus::Font font(&fontFamily, 24, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
                Gdiplus::SolidBrush solidBrush(Gdiplus::Color(255, 255, 0, 0));

                int capturedResultItemCount = result->GetItemsCount();

                if (capturedResultItemCount > 0)
                {
                    for (int j = 0; j < capturedResultItemCount; j++)
                    {
                        const CCapturedResultItem *capturedResultItem = result->GetItem(j);
                        CapturedResultItemType type = capturedResultItem->GetType();
                        if (type & CapturedResultItemType::CRIT_BARCODE)
                        {
                            const CBarcodeResultItem *barcodeResultItem = dynamic_cast<const CBarcodeResultItem *>(capturedResultItem);

                            CQuadrilateral location = barcodeResultItem->GetLocation();
                            Gdiplus::Point points[4];
                            points[0].X = location.points[0][0] * scale + 10;
                            points[0].Y = location.points[0][1] * scale + 50;
                            points[1].X = location.points[1][0] * scale + 10;
                            points[1].Y = location.points[1][1] * scale + 50;
                            points[2].X = location.points[2][0] * scale + 10;
                            points[2].Y = location.points[2][1] * scale + 50;
                            points[3].X = location.points[3][0] * scale + 10;
                            points[3].Y = location.points[3][1] * scale + 50;

                            graphics.DrawPolygon(&pen, points, 4);

                            wchar_t *newText = ConvertCharToWChar(barcodeResultItem->GetText());
                            graphics.DrawString(
                                newText, -1, &font,
                                Gdiplus::PointF(points[0].X, points[0].Y),
                                &solidBrush);
                            delete newText;
                        }
                    }
                }

                results.pop_back();
            }
        }

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_SIZE:
    {
        RECT rect;
        GetClientRect(hWnd, &rect);
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;

        if (hwndImagePanel)
        {
            SetWindowPos(hwndImagePanel, nullptr, 10, 10, width / 2 - 20, height - 20, SWP_NOZORDER);
        }

        if (hwndResultPanel)
        {
            SetWindowPos(hwndResultPanel, nullptr, width / 2 + 10, 10, width / 2 - 20, height - 20, SWP_NOZORDER);
        }

        if (hwndEdit)
        {
            SetWindowPos(hwndEdit, nullptr, 10, 40, width / 2 - 40, height - 70, SWP_NOZORDER);
        }

        InvalidateRect(hWnd, nullptr, TRUE);
    }
    break;

    case WM_DESTROY:
        if (cvr)
        {
            delete cvr;
        }

        if (g_pImage)
        {
            delete g_pImage;
        }

        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
