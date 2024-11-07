#ifndef CLIENT_H
#define CLIENT_H

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 2048

string WSAInitiate();
string getFile(SOCKET& socket, const string& filePath);
string handleMessage(const int port, const string& message, const string& ipAddress);

#endif // CLIENT_H