#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <filesystem>
#include <afxsock.h>

using namespace std;

#define PORT 8080
#define BUFFER_SIZE 2048

string getFile(CSocket& socket, const string& filePath) {
    CFile file;
    if (file.Open(wstring(filePath.begin(), filePath.end()).c_str(), CFile::modeCreate | CFile::modeWrite)) {
        CSocketFile socketFile(&socket);
        CArchive archive(&socketFile, CArchive::load);

        // Read data from buffer and write to file
        unsigned char buffer[BUFFER_SIZE];
        UINT bytesRead;
        while ((bytesRead = archive.Read(buffer, sizeof(buffer))) > 0) {
            file.Write(buffer, bytesRead);
        }
        file.Close();
        return filePath;
    }
    return string();
}

void handleMessage(const string& message, const string& ipAddress, string& receivedMessage, string& receivedFilePath) {
    CSocket client;
    // Initiate socket handle
    // [c++ - CSocket:: Create throwing exception in my MFC application - Stack Overflow](https://stackoverflow.com/a/2156303)
    client.m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); // To avoid CResourceException

    // Connect to server
    if (!client.Connect(wstring(ipAddress.begin(), ipAddress.end()).c_str(), PORT)) {
        cerr << "\nFailed to connect to server. Error: " << GetLastError() << endl;
    }
    cout << "\nConnected to the server!\n";

    // Send a message to server
    client.Send(message.c_str(), message.size() + 1);

    // Read and assign the message sent from server to 'receivedMessage' variable
    bool checked = 0;
    char buffer[BUFFER_SIZE]{ 0 };
    UINT bytesReceived;
    while ((bytesReceived = client.Receive(buffer, BUFFER_SIZE - 1))) {
        receivedMessage += buffer;
        
        if (!checked) {
            // Get the file sent from server if needed
            size_t pos = receivedMessage.find("See more in file");
            if (pos != string::npos) {
                receivedFilePath = getFile(client, string(filesystem::current_path().string() + "/" + receivedMessage.substr(pos + 17)));
                break;
            }
            checked = 1;
        }
    }

    // Close socket handle
    closesocket(client.m_hSocket);
    cout << "\nDisconnected from the server!\n";
}

#endif // !CLIENT_HPP