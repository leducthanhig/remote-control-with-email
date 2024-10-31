#ifndef CLIENT_H
#define CLIENT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 2048

string WSAInitiate();
string getFile(SOCKET& socket, const string& filePath);
string handleMessage(const int port, const string& message, const string& ipAddress, string& receivedMessage, string& receivedFilePath);

#endif // CLIENT_H