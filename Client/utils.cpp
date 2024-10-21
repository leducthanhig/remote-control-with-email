#include "utils.h"

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string sendHttpReq(string url, string headers, string method, string postFields) {
    CURL* curl;
    CURLcode res;
    curl_slist* pHeaders = NULL;
    string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (curl) {
        if (headers != "") {
            pHeaders = curl_slist_append(pHeaders, headers.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pHeaders);
        }
        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            response = string({ "curl_easy_perform() failed: ", curl_easy_strerror(res) });
        }
        curl_easy_cleanup(curl);
    }
    curl_slist_free_all(pHeaders);
    curl_global_cleanup();
	
	return response;
}

string refreshAccessToken() {
    ifstream fi1("data/credentials.json");
    json data1 = json::parse(fi1)["installed"];
    fi1.close();

    ifstream fi2("data/token.json");
    json data2 = json::parse(fi2);
    fi2.close();
    
    string url = "https://oauth2.googleapis.com/token";
    string clientId = data1["client_id"], clientSecret = data1["client_secret"], refreshToken = data2["refresh_token"];
    string postFields = "client_id=" + clientId + "&client_secret=" + clientSecret + "&refresh_token=" + refreshToken + "&grant_type=refresh_token";
    
    json responseData = json::parse(sendHttpReq(url, "", "POST", postFields));
    responseData["timestamp"] = time(0);
    responseData["refresh_token"] = refreshToken;

    ofstream fo("data/token.json");
    fo << responseData.dump(4);
    fo.close();

    return responseData["access_token"];
}

string getAuthorizationCode() {
    char alreadyHave;
    cout << "\nDo you already have a valid authorization code [y/N]? ";
    cin >> alreadyHave;
    
    string authorizationCode;
    if (tolower(alreadyHave) == 'y') {
        cout << "\nEnter your authorization code: ";
        cin >> authorizationCode;
    }
    else {
        ifstream fi("data/credentials.json");
        json data = json::parse(fi)["installed"];
        fi.close();
    
        string client_id = data["client_id"], redirect_uri = data["redirect_uris"][0];
        string url = "https://accounts.google.com/o/oauth2/auth?client_id=" + client_id + "&redirect_uri=" + redirect_uri + "&response_type=code&scope=https://www.googleapis.com/auth/gmail.modify";
        cout << "\nOpen this URL in your browser to login to the account that you want to be received mails:\n\n" << url 
            << "\n\nThen the URL will look something like this:\n\nhttp://localhost/?code=AUTHORIZATION_CODE\n\nJust copy the whole URL and paste here:\n";
    
        string urlWithCode;
        cin >> urlWithCode;
        string::iterator lastIterator;
        size_t pos = urlWithCode.find("&");
        if (pos == string::npos) {
            lastIterator = urlWithCode.end();
        }
        else {
            lastIterator = urlWithCode.begin() + pos;
        }
        authorizationCode = string(urlWithCode.begin() + urlWithCode.find("?code=") + 6, lastIterator);
    }
    return authorizationCode;
}

string getAccessToken() {
    ifstream fi1("data/token.json");
    if (fi1.is_open()) {
        json data = json::parse(fi1);
        fi1.close();

        if (data["expires_in"] - (time(0) - data["timestamp"]) > 10) {
            return data["access_token"];
        }
        return refreshAccessToken();
    }
    ifstream fi2("data/credentials.json");
    json data = json::parse(fi2)["installed"];
    fi2.close();

    string url = "https://oauth2.googleapis.com/token";
    string authCode = getAuthorizationCode(), clientId = data["client_id"], clientSecret = data["client_secret"], redirectUri = data["redirect_uris"][0];
    string postFields = "code=" + authCode + "&client_id=" + clientId + "&client_secret=" + clientSecret + "&redirect_uri=" + redirectUri + "&grant_type=authorization_code";
        
    string response = sendHttpReq(url, "", "POST", postFields);
    if (response.find("invalid_grant") != string::npos) {
        cout << "\nYour authorization code was expired. Please try again to get the new one!\n";
        return getAccessToken();
    }
    json responseData = json::parse(response);
    responseData["timestamp"] = time(0);

    ofstream fo("data/token.json");
    fo << responseData.dump(4);
    fo.close();

    return responseData["access_token"];
}

string getMessagesList(string query) {
    string accessToken = getAccessToken();
    string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages";
    string header = "Authorization: Bearer " + accessToken;
    
    string response = sendHttpReq(url + "?q=" + query, header);
    
    return response;
}

string getMessage(string id) {
    string accessToken = getAccessToken();
    string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/";
    string header = "Authorization: Bearer " + accessToken;
    string query = "?format=full";

    string response = sendHttpReq(url + id + query, header);

    return response;
}

string trashMessage(string id) {
    string accessToken = getAccessToken();
    string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/";
    string header = "Authorization: Bearer " + accessToken;

    string response = sendHttpReq(url + id + "/trash", header, "POST");

    return "\nTrashed successfully!";
}

string getMessageSubject(json message) {
    string subject = "";
    json headers = message["payload"]["headers"];
    for (json header: headers) {
        if (header["name"] == "Subject") {
            subject = header["value"];
            break;
        }
    }
    return subject;
}
