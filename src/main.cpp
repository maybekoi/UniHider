#define _CRT_SECURE_NO_WARNINGS
#define _WIN32_WINNT 0x0601
#include <iostream>
#include <string>
#include <zip.h>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <windows.h>
#include <shlobj.h>

namespace fs = std::filesystem;

// Forward declarations
bool hideVolume(const std::string& driveLetter);
bool hideTempDirectory(const std::string& tempDir);
bool createRamDrive(const std::string& driveLetter);
bool removeRamDrive(const std::string& driveLetter);

bool hideVolume(const std::string& driveLetter) {
    DWORD driveBit = 1 << (driveLetter[0] - 'A');
    
    HKEY hKey;
    LONG result = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideMyComputerIcons",
        0, NULL, 0, KEY_WRITE, NULL, &hKey, NULL);
        
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    result = RegSetValueExA(
        hKey,
        driveLetter.c_str(),
        0,
        REG_DWORD,
        (LPBYTE)&driveBit,
        sizeof(driveBit));
        
    RegCloseKey(hKey);
    
    if (result != ERROR_SUCCESS) {
        return false;
    }
    
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    
    return true;
}

bool hideTempDirectory(const std::string& tempDir) {
    DWORD attributes = GetFileAttributesA(tempDir.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        return false;
    }
    
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((tempDir + "\\*").c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string filename = findData.cFileName;
            if (filename != "." && filename != "..") {
                std::string fullPath = tempDir + "\\" + filename;
                if (!SetFileAttributesA(fullPath.c_str(),
                    FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 
                    FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)) {
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    if (!SetFileAttributesA(tempDir.c_str(),
        FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | 
        FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)) {
        return false;
    }

    return true;
}

bool removeHiddenVolume(const std::string& driveLetter) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\HideMyComputerIcons",
        0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        
        RegDeleteValueA(hKey, driveLetter.c_str());
        RegCloseKey(hKey);
        
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
    return true;
}

bool removeRamDrive(const std::string& driveLetter) {
    removeHiddenVolume(driveLetter);
    std::string cmd = "subst " + driveLetter + ": /D";
    system(cmd.c_str());    
    Sleep(100);
    return true;
}

bool createRamDrive(const std::string& driveLetter) {
    removeRamDrive(driveLetter);    
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string tempDir = std::string(tempPath) + "unihider_temp";
    
    try {
        fs::create_directories(tempDir);
    } catch (const std::exception& e) {
        return false;
    }
    
    if (!hideTempDirectory(tempDir)) {
    }
    
    std::string cmd = "subst " + driveLetter + ": \"" + tempDir + "\"";
    if (system(cmd.c_str()) != 0) {
        return false;
    }
    
    Sleep(500); 
    
    if (!hideVolume(driveLetter)) {
    }
    
    return true;
}

bool extractAndRun(const std::string& zipPath, const std::string& password, const std::string& exeName, const std::string& gameName) {
    const std::string ramDriveLetter = "R";
    bool success = false;
    
    try {
        std::cout << "Loading " << gameName << "..." << std::endl;
        int err = 0;
        zip* archive = zip_open(zipPath.c_str(), 0, &err);
        
        if (!archive) {
            return false;
        }

        zip_set_default_password(archive, password.c_str());

        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string tempDir = std::string(tempPath) + "unihider_temp\\";
        fs::create_directories(tempDir);
        
        hideTempDirectory(tempDir);
        
        if (!createRamDrive(ramDriveLetter)) {
            zip_close(archive);
            fs::remove_all(tempDir);
            return false;
        }

        struct zip_stat st;
        zip_stat_init(&st);
        const zip_int64_t num_entries = zip_get_num_entries(archive, 0);
        std::string exePath;

        for (zip_int64_t i = 0; i < num_entries; i++) {
            if (zip_stat_index(archive, i, 0, &st) == 0) {
                
                std::string fullPath = ramDriveLetter + ":\\" + st.name;
                fs::path dirPath = fs::path(fullPath).parent_path();
                fs::create_directories(dirPath);

                zip_file* file = zip_fopen_index(archive, i, 0);
                if (file) {
                    std::vector<char> buffer(st.size);
                    if (zip_fread(file, buffer.data(), st.size) > 0) {
                        FILE* out = fopen(fullPath.c_str(), "wb");
                        if (out) {
                            fwrite(buffer.data(), 1, st.size, out);
                            fclose(out);

                            if (std::string(st.name) == exeName) {
                                exePath = fullPath;
                            }
                        } else {
                        }
                    } else {
                    }
                    zip_fclose(file);
                } else {
                }
            }
        }

        zip_close(archive);

        if (!exePath.empty()) {
            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi;
            
            if (CreateProcessA(
                exePath.c_str(),
                NULL,
                NULL,
                NULL,
                FALSE,
                0,
                NULL,
                fs::path(exePath).parent_path().string().c_str(),
                &si,
                &pi
            )) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                success = true;
            }
        }

        removeRamDrive(ramDriveLetter);
        fs::remove_all(tempDir);
        
        return success;
    }
    catch (...) {
        removeRamDrive(ramDriveLetter);
        return false;
    }
}

BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT || 
        signal == CTRL_CLOSE_EVENT || signal == CTRL_LOGOFF_EVENT || 
        signal == CTRL_SHUTDOWN_EVENT) {
        removeRamDrive("R");
        return TRUE;
    }
    return FALSE;
}

int main() {
    const std::string zipFile = "game.dat";  // Change this to your zip file name
    const std::string password = "your_password_here";  // Change this to your zip password
    const std::string exeName = "HappyStealingWithMarisaKirisame.exe";  // Change this to your game's exe name
    const std::string gameName = "Happy Stealing With Marisa Kirisame"; // Change this to your game's name    
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
        
    if (!extractAndRun(zipFile, password, exeName, gameName)) {
        std::cerr << "Failed to extract and run the game" << std::endl;
        removeRamDrive("R");
        return 1;
    }
    
    removeRamDrive("R"); 
    return 0;
} 