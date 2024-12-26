#include "client.hpp"

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
        oss << "Failed to open file: " << filePath << ".";
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

string handleMessage(const int port, const string& message, const string& ipAddress) {
    ostringstream oss;
    
    // Create socket
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        oss << "Failed to create socket: " << WSAGetLastError() << ".";
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
        return oss.str();
    }

    // Send message to server
    send(clientSocket, message.c_str(), static_cast<int>(message.size()) + 1, 0);

    // Receive message from server
    string receivedMessage, receivedFilePath;
    char buffer[BUFFER_SIZE];
    int bytesReceived;
    bool checked = 0;
    while ((bytesReceived = recv(clientSocket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        receivedMessage += buffer;

        if (!checked) {
            // Check for "See more in file" and retrieve file if found
            size_t pos = receivedMessage.find("See more in file");
            if (pos != string::npos) {
                string filePath = filesystem::current_path().string() + "\\received_" + receivedMessage.substr(pos + 17);
                receivedMessage.replace(receivedMessage.begin() + pos + 12, receivedMessage.end(), "the attachment file.");
                receivedFilePath = getFile(clientSocket, filePath);
                break;
            }
            checked = 1;
        }
    }

    // Shutdown the connection
    if (shutdown(clientSocket, SD_SEND) == SOCKET_ERROR) {
        oss << "Failed to shutdown the connection: " << WSAGetLastError() << ".";
        closesocket(clientSocket);
        return oss.str();
    }

    // Close socket
    closesocket(clientSocket);

    return receivedMessage + "\nreceivedFilePath=" + receivedFilePath;
}