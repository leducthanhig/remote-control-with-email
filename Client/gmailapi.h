#ifndef GMAILAPI_HPP
#define GMAILAPI_HPP

#include <ctime>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <curl/curl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;

#define CREDENTIALS_PATH    "data/credentials.json"
#define TOKENS_PATH         "data/tokens.json"

#define GOOGLEAPI_AUTH_SCOPE_URL        "https://www.googleapis.com/auth/gmail.modify"
#define GOOGLEAPI_GMAIL_MESSAGES_URL    "https://gmail.googleapis.com/gmail/v1/users/me/messages"
#define GOOGLEAPI_GMAIL_PROFILE_URL     "https://gmail.googleapis.com/gmail/v1/users/me/profile"

string base64Encode(const string& data);
size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
string sendHttpReq(const string& url, const vector<string>& headers = {}, const string& body = "", const string& method = "GET");
string refreshAccessToken();
string getAccessToken(const string& authorizationCode = "");
string getMailList(const string& query);
string getMail(const string& id);
string trashMail(const string& id);
string getUserEmailAddress();
string getMailSubject(const json& message);
string sendMail(const string& to, const string& from, const string& subject, const string& messageBody, const string& filePath);

#endif // GMAILAPI_HPP