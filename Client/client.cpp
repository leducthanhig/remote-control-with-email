#include "client.h"

string WSAInitiate() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        ostringstream oss;
        oss << "WSAStartup failed: " << WSAGetLastError() << ".";
        return oss.str();
    }
    return "";
}

string getFile(SOCKET& socket, const string& filePath) {
    ostringstream oss;

    // Open file to write received data
    ofstream file(filePath, ios::binary);
    if (!file.is_open()) {
        oss << "Failed to open file: " << filePath << endl;
        return oss.str();
    }

    // Receive data from socket and write to file
    char buffer[BUFFER_SIZE];
    int bytesRead;
    while ((bytesRead = recv(socket, buffer, sizeof(buffer), 0)) > 0) {
        file.write(buffer, bytesRead);
    }
    file.close();

    return filePath;
}

string handleMessage(const int port, const string& message, const string& ipAddress, string& receivedMessage, string& receivedFilePath) {
    ostringstream oss;
    
    // Create socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        oss << "Failed to create socket: " << WSAGetLastError() << ".";
        WSACleanup();
        return oss.str();
    }

    // Setup server information
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    inet_pton(AF_INET, ipAddress.c_str(), &serverAddr.sin_addr);

    // Connect to server
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        oss << "Failed to connect to server: " << WSAGetLastError() << ".";
        closesocket(clientSocket);
        WSACleanup();
        return oss.str();
    }
    oss << "Connected to the server!";

    // Send message to server
    send(clientSocket, message.c_str(), message.size() + 1, 0);

    // Receive message from server
    char buffer[BUFFER_SIZE] = { 0 };
    int bytesReceived;
    bool checked = 0;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        receivedMessage += buffer;

        if (!checked) {
            // Check for "See more in file" and retrieve file if found
            size_t pos = receivedMessage.find("See more in file");
            if (pos != string::npos) {
                string filePath = filesystem::current_path().string() + "/" + receivedMessage.substr(pos + 17);
                receivedFilePath = getFile(clientSocket, filePath);
                break;
            }
            checked = 1;
        }
    }

    // Close socket
    oss << "Disconnected from the server!";
    closesocket(clientSocket);
    WSACleanup();

    return oss.str();
}