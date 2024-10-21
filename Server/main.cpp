#include "utils.h"

int main() {
    cout << "o--------------------- INSTRUCTIONS ---------------------o\n";
    cout << "|                                                        |\n";
    cout << "|   list    [app|service]                                |\n";
    cout << "|   start   [app|service]      [appPath|serviceName]     |\n";
    cout << "|   stop    [app|service]      [processID|serviceName]   |\n";
    cout << "|   power   [shutdown|restart]                           |\n";
    cout << "|   file    [copy|delete]      srcPath [desPath]         |\n";
    cout << "|   capture screen             outPath                   |\n";
    cout << "|   start   [camera|keylogger]                           |\n";
    cout << "|   stop    [camera|keylogger]                           |\n";
    cout << "|   exit                                                 |\n";
    cout << "|                                                        |\n";
    cout << "o--------------------------------------------------------o\n";
    
    map<string, map<string, function<void()>>> handlers = setupHandlers();
    while (1) {
        string cmd, opt;
        cout << "\nEnter your command:\n\n>>> ";
        cin >> cmd;
        if (cmd == "exit") break;
        cin >> opt;
        
        try {
            handlers[cmd][opt]();
        }
        catch (...) {
            cerr << "\nError: " << GetLastError() << endl;
        }
    }
    return 0;
}