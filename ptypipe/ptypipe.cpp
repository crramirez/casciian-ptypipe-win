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
        if (!WriteFile(hOutput, buffer, bytesRead, &bytesWritten, NULL) || bytesWritten != bytesRead) {
            break;
        }
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
    // Set console input and output code pages to UTF-8
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // Check if command line arguments are provided
    if (argc < 2) {
        std::wcerr << L"Usage: ptypipe.exe <command> [args...]" << std::endl;
        std::wcerr << L"Example: ptypipe.exe cmd.exe" << std::endl;
        std::wcerr << L"Example: ptypipe.exe powershell.exe" << std::endl;
        return 1;
    }

    // Build the command line from arguments using proper Windows escaping
    std::wstring cmdLine;
    for (int i = 1; i < argc; i++) {
        if (i > 1) cmdLine += L" ";
        
        // Convert argument from narrow to wide string using UTF-8
        std::string narrowArg = argv[i];
        int wideSize = MultiByteToWideChar(CP_UTF8, 0, narrowArg.c_str(), -1, NULL, 0);
        if (wideSize == 0) {
            std::wcerr << L"Failed to convert argument to wide string" << std::endl;
            return 1;
        }
        std::vector<wchar_t> wideArg(wideSize);
        MultiByteToWideChar(CP_UTF8, 0, narrowArg.c_str(), -1, wideArg.data(), wideSize);
        std::wstring arg(wideArg.data());
        
        // Check if argument needs quoting (contains space, tab, or quote)
        bool needsQuotes = arg.find(L' ') != std::wstring::npos || 
                          arg.find(L'\t') != std::wstring::npos ||
                          arg.find(L'"') != std::wstring::npos;
        
        if (needsQuotes) {
            cmdLine += L'"';
        }
        
        // Escape the argument per Windows rules:
        // Backslashes are literal except when followed by a quote or at the end before closing quote
        for (size_t j = 0; j < arg.length(); j++) {
            size_t numBackslashes = 0;
            
            // Count consecutive backslashes
            while (j < arg.length() && arg[j] == L'\\') {
                numBackslashes++;
                j++;
            }
            
            if (j == arg.length()) {
                // Backslashes at end of arg: double them if we're quoting
                cmdLine.append(needsQuotes ? numBackslashes * 2 : numBackslashes, L'\\');
                break;
            } else if (arg[j] == L'"') {
                // Backslashes before quote: double them and escape the quote
                cmdLine.append(numBackslashes * 2, L'\\');
                cmdLine += L"\\\"";
            } else {
                // Normal backslashes: keep as-is
                cmdLine.append(numBackslashes, L'\\');
                cmdLine += arg[j];
            }
        }
        
        if (needsQuotes) {
            cmdLine += L'"';
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
        std::wcerr << L"Failed to create stdin pipe" << std::endl;
        return 1;
    }
    // Ensure the write handle to stdin is not inherited
    SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);

    // Create stdout pipe
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0)) {
        std::wcerr << L"Failed to create stdout pipe" << std::endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        return 1;
    }
    // Ensure the read handle to stdout is not inherited
    SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);

    // Create stderr pipe
    if (!CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0)) {
        std::wcerr << L"Failed to create stderr pipe" << std::endl;
        CloseHandle(hStdInRead);
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        CloseHandle(hStdOutWrite);
        return 1;
    }
    // Ensure the read handle to stderr is not inherited
    SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);

    // Setup process startup information
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdInput = hStdInRead;
    si.hStdOutput = hStdOutWrite;
    si.hStdError = hStdErrWrite;
    si.wShowWindow = SW_HIDE; // Hide the window

    ZeroMemory(&pi, sizeof(pi));

    // Create the child process using Unicode API
    std::vector<wchar_t> cmdLineBuffer(cmdLine.begin(), cmdLine.end());
    cmdLineBuffer.push_back(L'\0');

    // Build a Unicode environment block for UTF-8 support in child processes
    // Get current environment and append UTF-8 related variables
    std::wstring envBlock;
    wchar_t* currentEnv = GetEnvironmentStringsW();
    if (currentEnv) {
        // Copy existing environment variables
        wchar_t* p = currentEnv;
        while (*p) {
            size_t len = wcslen(p);
            envBlock.append(p, len + 1);  // Include null terminator
            p += len + 1;
        }
        FreeEnvironmentStringsW(currentEnv);
    }

    // Add environment variables to encourage UTF-8 output in child processes
    // Only add if the variable is not already set in the environment
    // PYTHONIOENCODING for Python scripts
    if (GetEnvironmentVariableW(L"PYTHONIOENCODING", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
        envBlock += L"PYTHONIOENCODING=utf-8";
        envBlock += L'\0';
    }
    // PYTHONUTF8 for Python 3.7+
    if (GetEnvironmentVariableW(L"PYTHONUTF8", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
        envBlock += L"PYTHONUTF8=1";
        envBlock += L'\0';
    }
    // LANG for Unix-like programs and some cross-platform tools
    if (GetEnvironmentVariableW(L"LANG", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
        envBlock += L"LANG=en_US.UTF-8";
        envBlock += L'\0';
    }
    // LC_ALL for locale settings
    if (GetEnvironmentVariableW(L"LC_ALL", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
        envBlock += L"LC_ALL=en_US.UTF-8";
        envBlock += L'\0';
    }
    // OutputEncoding for PowerShell when running scripts
    if (GetEnvironmentVariableW(L"PSDefaultParameterValues", NULL, 0) == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
        envBlock += L"PSDefaultParameterValues=@{\"Out-File:Encoding\"=\"utf8\"}";
        envBlock += L'\0';
    }
    // Add final null terminator for the environment block
    envBlock += L'\0';

    if (!CreateProcessW(
        NULL,                       // Application name
        cmdLineBuffer.data(),       // Command line (mutable wide string buffer)
        NULL,                       // Process security attributes
        NULL,                       // Thread security attributes
        TRUE,                       // Inherit handles
        CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,  // Creation flags - no window, Unicode environment
        (LPVOID)envBlock.c_str(),   // Environment with UTF-8 settings
        NULL,                       // Current directory
        &si,                        // Startup info
        &pi                         // Process information
    )) {
        std::wcerr << L"Failed to create process. Error: " << GetLastError() << std::endl;
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
    // stdout and stderr threads will finish when child process closes its handles
    stdoutThread.join();
    stderrThread.join();
    
    // Cancel any pending synchronous I/O on the stdin thread so it can exit cleanly
    CancelSynchronousIo(stdinThread.native_handle());
    stdinThread.join();

    return exitCode;
}
