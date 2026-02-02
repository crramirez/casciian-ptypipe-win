# casciian-ptypipe-win
Pseudo terminal for Windows

## Description

`ptypipe.exe` is a Windows console application that creates a hidden child process and redirects its stdin, stdout, and stderr to the parent process. This allows you to run commands like cmd.exe or powershell.exe in a hidden window while maintaining full I/O control.

## Features

- Creates a hidden child process (no visible window)
- Redirects stdin from the parent process to the child process
- Redirects stdout from the child process to the parent process
- Redirects stderr from the child process to the parent process stderr
- Supports any command-line application

## Building

### Requirements
- Visual Studio 2022 or later (with C++ desktop development workload)
- Windows SDK 10.0 or later

### Build Instructions

1. Open `ptypipe.sln` in Visual Studio
2. Select your desired configuration (Debug/Release) and platform (x86/x64)
3. Build the solution (Ctrl+Shift+B or Build → Build Solution)
4. The executable will be in `x64\Release\ptypipe.exe` or `x64\Debug\ptypipe.exe`

Alternatively, build from the command line using the provided batch script:
```
build.bat [Configuration] [Platform]
```

Examples:
```
build.bat Release x64
build.bat Debug x86
```

Or use MSBuild directly:
```
msbuild ptypipe.sln /p:Configuration=Release /p:Platform=x64
```

## Usage

```
ptypipe.exe <command> [args...]
```

### Examples

Run cmd.exe in a hidden window:
```
ptypipe.exe cmd.exe
```

Run PowerShell in a hidden window:
```
ptypipe.exe powershell.exe
```

Run a command with arguments:
```
ptypipe.exe cmd.exe /c dir
```

## How It Works

1. The application parses command-line arguments to determine which command to execute
2. Creates anonymous pipes for stdin, stdout, and stderr
3. Launches the child process with `CREATE_NO_WINDOW` and `SW_HIDE` flags to keep it hidden
4. Spawns threads to handle bidirectional I/O:
   - One thread reads from parent stdin and writes to child stdin
   - One thread reads from child stdout and writes to parent stdout
   - One thread reads from child stderr and writes to parent stderr
5. Waits for the child process to complete
6. Returns the exit code of the child process

## License

See LICENSE file for details.
