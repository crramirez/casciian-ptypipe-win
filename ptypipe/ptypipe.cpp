#include <windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

// Function to read from a pipe and write to a handle (used for stdout/stderr redirection)
void PipeReaderThread(HANDLE hReadPipe, HANDLE hOutput) {
    const DWORD BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead, bytesWritten;

    while (true) {
        if (!ReadFile(hReadPipe, buffer, BUFFER_SIZE, &bytesRead, NULL) || bytesRead == 0) {
            break;
        }
        WriteFile(hOutput, buffer, bytesRead, &bytesWritten, NULL);
        FlushFileBuffers(hOutput);
    }
}

// Function to read from stdin and write to a pipe (used for stdin redirection)
void StdinWriterThread(HANDLE hWritePipe, HANDLE hInput) {
    const DWORD BUFFER_SIZE = 4096;
    char buffer[BUFFER_SIZE];
    DWORD bytesRead, bytesWritten;

    while (true) {
        if (!ReadFile(hInput, buffer, BUFFER_SIZE, &bytesRead, NULL) || bytesRead == 0) {
            break;
        }
        if (!WriteFile(hWritePipe, buffer, bytesRead, &bytesWritten, NULL)) {
            break;
        }
    }
    // Close the write pipe when stdin is closed
    CloseHandle(hWritePipe);
}

int main(int argc, char* argv[]) {
    // Check if command line arguments are provided
    if (argc < 2) {
        std::cerr << "Usage: ptypipe.exe <command> [args...]" << std::endl;
        std::cerr << "Example: ptypipe.exe cmd.exe" << std::endl;
        std::cerr << "Example: ptypipe.exe powershell.exe" << std::endl;
        return 1;
    }

    // Build the command line from arguments
    std::string cmdLine;
    for (int i = 1; i < argc; i++) {
        if (i > 1) cmdLine += " ";
        
        // Quote the argument if it contains spaces
        std::string arg = argv[i];
        if (arg.find(' ') != std::string::npos) {
            cmdLine += "\"" + arg + "\"";
        } else {
            cmdLine += arg;
        }
    }

    // Create pipes for stdin, stdout, stderr
    HANDLE hStdInRead = NULL, hStdInWrite = NULL;
    HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
    HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    // Create stdin pipe
    if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0)) {
        std::cerr << "Failed to create stdin pipe" << std::endl;
        return 1;
    }
    // Ensure the write handle to stdin is not inherited
    SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);

    // Create stdout pipe
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        std::cerr << "Failed to create stdout pipe" << std::endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        return 1;
    }
    // Ensure the read handle to stdout is not inherited
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    // Create stderr pipe
    if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        std::cerr << "Failed to create stderr pipe" << std::endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return 1;
    }
    // Ensure the read handle to stderr is not inherited
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    // Setup process startup information
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdInRead;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.wShowWindow = SW_HIDE; // Hide the window

    ZeroMemory(&pi, sizeof(pi));

    // Create the child process
    std::vector<char> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back('\0');

    if (!CreateProcessA(
        NULL,                       // Application name
        cmdLineBuffer.data(),       // Command line
        NULL,                       // Process security attributes
        NULL,                       // Thread security attributes
        TRUE,                       // Inherit handles
        CREATE_NO_WINDOW,           // Creation flags - no window
        NULL,                       // Environment
        NULL,                       // Current directory
        &si,                        // Startup info
        &pi                         // Process information
    )) {
        std::cerr << "Failed to create process. Error: " << GetLastError() << std::endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        CloseHandle(hStdErrRead);
        CloseHandle(hStdErrWrite);
        return 1;
    }

    // Close handles that are used by the child process
    CloseHandle(hStdInRead);
    CloseHandle(hStdOutWrite);
    CloseHandle(hStdErrWrite);

    // Get stdin/stdout/stderr handles of the parent process
    HANDLE hParentStdIn = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hParentStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hParentStdErr = GetStdHandle(STD_ERROR_HANDLE);

    // Create threads for I/O redirection
    std::thread stdinThread(StdinWriterThread, hStdInWrite, hParentStdIn);
    std::thread stdoutThread(PipeReaderThread, hStdOutRead, hParentStdOut);
    std::thread stderrThread(PipeReaderThread, hStdErrRead, hParentStdErr);

    // Wait for the process to complete
    WaitForSingleObject(pi.hProcess, INFINITE);

    // Get exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    // Close process and thread handles
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // Close remaining pipe handles
    CloseHandle(hStdOutRead);
    CloseHandle(hStdErrRead);

    // Wait for I/O threads to complete
    // Note: stdin thread might be blocked on ReadFile, so we don't wait for it
    stdoutThread.join();
    stderrThread.join();
    
    // Detach stdin thread as it might be blocked
    stdinThread.detach();

    return exitCode;
}
