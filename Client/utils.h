#ifndef UTILS_H
#define UTILS_H

#include <ctime>
#include <thread>
#include <chrono>
#include <string>
#include <fstream>
#include <iostream>
#include <afxsock.h>
#include <windows.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;

#define SLEEP_TIME 30
#define PORT 8080
#define BUFFER_SIZE 500000

void connectAndSendMessage(wstring message, wstring ipAddress);
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
string sendHttpReq(string url, string headers = "", string method = "GET", string postFields = "");
string refreshAccessToken();
string getAuthorizationCode();
string getAccessToken();
string getMessagesList(string query = "");
string getMessage(string id);
string trashMessage(string id);
string getMessageSubject(json& message);

#endif