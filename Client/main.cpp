#include "utils.h"

#define SLEEP 60

int main() {
    system("chcp 65001");
    system("cls");
    
    string acceptAddress;
    cout << "Enter the email address that you will use to send mails: ";
    cin >> acceptAddress;
    string q = "from:" + acceptAddress + "%20is:unread";
    
    while (1) {
        try {
            cout << "\nChecking mailbox...\n";
            string response = "";
            response = getMessagesList(q);
            json responseData = json::parse(response);

            if (responseData["resultSizeEstimate"] != 0) {
                cout << "\nGetting mail's data...\n";
                response = getMessage(responseData["messages"][0]["id"]);
                string subject = getMessageSubject(json::parse(response));
                string body = json::parse(response)["snippet"];
            
                if (subject == "") subject = "(empty)";
                if (body == "") body = "(empty)";
                cout << "\nFrom: " << acceptAddress << "\nSubject: " << subject << "\nBody: " << body << endl;
            
                cout << "\nTrashing mail...\n";
                response = trashMessage(responseData["messages"][0]["id"]);
                cout << response << endl;
            }
            else {
                cout << "\nNo unread mails found!\n";
            }
        }
        catch (...) {
            cerr << "\nError: " << GetLastError() << endl;
        }
        cout << "\nSleeping for " << SLEEP << " seconds...\n";
        this_thread::sleep_for(chrono::seconds(SLEEP));
    }
    return 0;
}