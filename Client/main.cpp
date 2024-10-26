#include <iostream>
#include <thread>
#include <chrono>
#include "client.hpp"
#include "gmailapi.hpp"

using namespace std;

#define SLEEP_TIME 30

int main() {
    system("chcp 65001");
    system("cls");
    
    // Initiate socket
    if (!AfxSocketInit()) {
        cerr << "\nFailed to initialize sockets. Error: " << GetLastError() << endl;
        return 0;
    }

    /*string cmd;
    while (cmd.find("exit") == string::npos) {
        cout << "\nEnter command: ";
        getline(cin, cmd);
        string receivedMessage, receivedFilePath;
        handleMessage(cmd, "127.0.0.1", receivedMessage, receivedFilePath);
        cout << receivedMessage << "\n" << receivedFilePath << endl;
    }*/

//#if 0
    // Get accept mail address
    string acceptAddress;
    cout << "Enter the email address that you will use to send mails: ";
    cin >> acceptAddress;
    // Create mail filter query
    string q = "from:" + acceptAddress + "%20is:unread";
    
    while (1) {
        try {
            // Get mail list from inbox
            cout << "\nChecking mailbox...\n";
            string response = "";
            response = getMailList(q);
            json responseData = json::parse(response);

            if (responseData["resultSizeEstimate"] != 0) {
                // Get mail data
                cout << "\nGetting mail's data...\n";
                response = getMail(responseData["messages"][0]["id"]);
                json response_json = json::parse(response);
                
                // Get and print out the mail subject and a snippet of the body
                string subject = getMailSubject(response_json);
                string body = response_json["snippet"];
                if (subject == "") subject = "(empty)";
                if (body == "") body = "(empty)";
                cout << "\nFrom: " << acceptAddress << "\nSubject: " << subject << "\nBody: " << body << endl;
            
                // Connect to server for sending message
                string receivedMessage, receivedFilePath;
                handleMessage(subject, body, receivedMessage, receivedFilePath);

                // Trash the handled mail
                cout << "\nTrashing mail...\n";
                response = trashMail(responseData["messages"][0]["id"]);
                if (response.find("error") == string::npos) {
                    cout << "\nTrashed mail successfully.\n";
                }
                
                // Send back to the sender the server message
                response = sendMail(acceptAddress, getUserEmailAddress(), "Server response", receivedMessage, receivedFilePath);
                if (response.find("error") == string::npos) {
                    cout << "\Sent mail successfully.\n";
                }
                
                // Exit loop when received 'exit' message
                if (response.find("exit") == 0) break;
            }
            else {
                cout << "\nNo unread mails found!\n";
            }
        }
        catch (const exception& e) {
            cerr << "\nException: " << e.what() << endl;
        }
        // Sleep for a specific time to reduce the resource usages
        cout << "\nSleeping for " << SLEEP_TIME << " seconds...\n";
        this_thread::sleep_for(chrono::seconds(SLEEP_TIME));
    }
//#endif
    return 0;
}