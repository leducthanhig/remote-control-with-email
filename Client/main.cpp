#include "utils.h"

int main() {
    system("chcp 65001");
    system("cls");
    
    if (!AfxSocketInit()) {
        cerr << "\nFailed to initialize sockets. Error: " << GetLastError() << endl;
        return 0;
    }

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
                json response_json = json::parse(response);
                
                string subject = getMessageSubject(response_json);
                string body = response_json["snippet"];
                if (subject == "") subject = "(empty)";
                if (body == "") body = "(empty)";
                cout << "\nFrom: " << acceptAddress << "\nSubject: " << subject << "\nBody: " << body << endl;
            
                connectAndSendMessage(wstring(subject.begin(), subject.end()), wstring(body.begin(), body.end()));

                cout << "\nTrashing mail...\n";
                response = trashMessage(responseData["messages"][0]["id"]);
                cout << response << endl;
            }
            else {
                cout << "\nNo unread mails found!\n";
            }
        }
        catch (const exception& e) {
            cerr << "\nException: " << e.what() << endl;
        }
        cout << "\nSleeping for " << SLEEP_TIME << " seconds...\n";
        this_thread::sleep_for(chrono::seconds(SLEEP_TIME));
    }
    return 0;
}