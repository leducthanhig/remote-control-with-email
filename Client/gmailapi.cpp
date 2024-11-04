#include "gmailapi.h"

// Function to encode email in Base64
string base64Encode(const string& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);

    BIO_write(bio, data.c_str(), static_cast<int>(data.size()));
    BIO_flush(bio);

    char* encoded = nullptr;
    long len = BIO_get_mem_data(bio, &encoded);

    string result(encoded, len);

    BIO_free_all(bio);
    return result;
}

// Helper function to handle the response
size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

string sendHttpReq(const string& url, const vector<string>& headers, const string& body, const string& method) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    CURL* curl = curl_easy_init();
    curl_slist* pHeaders = nullptr;
    string response;

    // Add headers
    for (string header : headers) {
        pHeaders = curl_slist_append(pHeaders, header.c_str());
    }

    // Set necessary options
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, pHeaders);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    // Set request body
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    // Perform requesting
    CURLcode res = curl_easy_perform(curl);
    if (res == CURLE_COULDNT_RESOLVE_HOST) {
        throw runtime_error("Check your internet connection and try again!");
    }
    else if (res != CURLE_OK) {
        throw runtime_error("curl_easy_perform() failed!");
    }

    // Clean up
    curl_slist_free_all(pHeaders);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    return response;
}

string refreshAccessToken() {
    // Get credential informations from file
    ifstream fi(CREDENTIALS_PATH);
    json credentials = json::parse(fi)["installed"];
    fi.close();

    // Get token informations from file
    fi.open(TOKENS_PATH);
    json tokens = json::parse(fi);
    fi.close();

    string url = credentials["token_uri"],
        client_id = credentials["client_id"],
        client_secret = credentials["client_secret"],
        refresh_token = tokens["refresh_token"],
        reqBody = "client_id=" + client_id + "&client_secret=" + client_secret
        + "&refresh_token=" + refresh_token + "&grant_type=refresh_token";

    // Send a post request to Google API
    string response = sendHttpReq(url, {}, reqBody, "POST");

    // Add timestamp and refresh token to the response data
    json response_json = json::parse(response);
    response_json["timestamp"] = time(0);
    response_json["refresh_token"] = refresh_token;

    // Save token information to file
    ofstream fo(TOKENS_PATH);
    fo << response_json.dump(4);
    fo.close();

    return response_json["access_token"];
}

string getAccessToken(const string& authorizationCode) {
    // Check if the file that contains token informations exist
    ifstream fi(TOKENS_PATH);
    if (fi.is_open()) {
        json tokens = json::parse(fi);
        fi.close();

        // Check if the access token is expired
        if (tokens["expires_in"] - (time(0) - tokens["timestamp"]) > 10) {
            return tokens["access_token"];
        }
        string accessToken = refreshAccessToken();
        if (accessToken != "") return accessToken;
    }

    // Get credential information from file
    fi.open(CREDENTIALS_PATH);
    json credentials = json::parse(fi)["installed"];
    fi.close();

    string url = credentials["token_uri"],
        authCode = authorizationCode,
        clientId = credentials["client_id"],
        clientSecret = credentials["client_secret"],
        redirectUri = credentials["redirect_uris"][0],
        reqBody = "code=" + authCode + "&client_id=" + clientId + "&client_secret=" + clientSecret
        + "&redirect_uri=" + redirectUri + "&grant_type=authorization_code";

    // Send a post request to Google API
    string response = sendHttpReq(url, {}, reqBody, "POST");

    // Add timestamp to the response data
    json response_json = json::parse(response);
    response_json["timestamp"] = time(0);

    // Save the token information to file
    ofstream fo(TOKENS_PATH);
    fo << response_json.dump(4);
    fo.close();

    return response_json["access_token"];
}

string getMailList(const string& query) {
    string url = GOOGLEAPI_GMAIL_MESSAGES_URL"?q=" + query;
    return sendHttpReq(url, { "Authorization: Bearer " + getAccessToken() });
}

string getMail(const string& id) {
    string url = GOOGLEAPI_GMAIL_MESSAGES_URL"/" + id + "?format=full";
    return sendHttpReq(url, { "Authorization: Bearer " + getAccessToken() });
}

string trashMail(const string& id) {
    string url = GOOGLEAPI_GMAIL_MESSAGES_URL"/" + id + "/trash";
    return sendHttpReq(url, { "Authorization: Bearer " + getAccessToken() }, "", "POST");
}

string getUserEmailAddress() {
    // Get token informations from file
    ifstream tokensFile(TOKENS_PATH);
    json tokens = json::parse(tokensFile);
    tokensFile.close();
    
    // Check if the account name is in the token file
    if (tokens.find("account") == tokens.end()) {
        // Get user email address from Gmail API
        json response_json = json::parse(sendHttpReq(GOOGLEAPI_GMAIL_PROFILE_URL, { "Authorization: Bearer " + getAccessToken() }));
        
        // Add user email address to token informations
        tokens["account"] = response_json["emailAddress"];
        
        // Save the updated information to file
        ofstream tokensFile(TOKENS_PATH);
        tokensFile << tokens.dump(4);
        tokensFile.close();
        
        return response_json["emailAddress"];
    }
    else {
        return tokens["account"];
    }
}

string getMailSubject(const json& message) {
    // Find the header that contains the mail subject
    const json& headers = message["payload"]["headers"];
    for (const json& header : headers) {
        if (header["name"] == "Subject") {
            return header["value"];
        }
    }
    return "";
}

string sendMail(const string& to, const string& from, const string& subject, const string& messageBody, const string& filePath) {
    ostringstream emailStream;

    // Set email headers
    emailStream << "From: " << from << "\r\n"
        << "To: " << to << "\r\n"
        << "Subject: " << subject << "\r\n"
        << "MIME-Version: 1.0\r\n"
        << "Content-Type: multipart/mixed; boundary=\"boundary_string\"\r\n"
        << "\r\n--boundary_string\r\n";

    // Add email body
    emailStream << "Content-Type: text/plain; charset=\"UTF-8\"\r\n"
        << "\r\n" << messageBody << "\r\n"
        << "\r\n--boundary_string\r\n";

    // Add attachment (if any)
    if (filePath.size()) {
        string fileName = filePath.substr(filePath.find_last_of('/') + 1);
        if (filePath.find(".txt") + 4 == filePath.size()) {
            // Add text file attachment
            emailStream << "Content-Type: text/plain; name=\"" << fileName << "\"\r\n"
                << "Content-Disposition: attachment; filename=\"" << fileName << "\"\r\n"
                << "Content-Transfer-Encoding: base64\r\n"
                << "\r\n";

            ifstream textFile(filePath, ios::binary);
            ostringstream textData;
            textData << textFile.rdbuf();
            emailStream << base64Encode(textData.str()) << "\r\n--boundary_string\r\n";
        }
        else {
            // Add image file attachment
            emailStream << "Content-Type: image/png; name=\"" << fileName << "\"\r\n"
                << "Content-Disposition: attachment; filename=\"" << fileName << "\"\r\n"
                << "Content-Transfer-Encoding: base64\r\n"
                << "\r\n";

            ifstream imageFile(filePath, ios::binary);
            ostringstream imageData;
            imageData << imageFile.rdbuf();
            emailStream << base64Encode(imageData.str()) << "\r\n--boundary_string--";
        }
    }

    // Send mail
    string reqBody = "{\"raw\":\"" + base64Encode(emailStream.str()) + "\"}";
    vector<string> headers = { "Authorization: Bearer " + getAccessToken(), "Content-Type: application/json" };
    return sendHttpReq(GOOGLEAPI_GMAIL_MESSAGES_URL"/send", headers, reqBody, "POST");
}