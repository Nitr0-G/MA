#pragma once

#include <iostream>
#include <Windows.h>
#include <fstream>

class ScreenShot_
{
private:
	int SendFileOverSocket(SOCKET clientSocket, const char* filePath);
	int SaveBitmapToFile(HBITMAP hBitmap, const char* fileName);
public:
	int CaptureScreenshot(SOCKET clientSocket);
};

//int SendFileOverSocket(SOCKET clientSocket, const char* filePath);
//int SaveBitmapToFile(HBITMAP hBitmap, const char* fileName);
//int CaptureScreenshot(SOCKET clientSocket);