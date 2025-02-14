#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <zip.h>
#include <filesystem>
#include <vector>
#include <cstdlib>
#include <windows.h>

namespace fs = std::filesystem;

bool extractAndRun(const std::string& zipPath, const std::string& password, const std::string& exeName) {
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

    struct zip_stat st;
    zip_stat_init(&st);
    const zip_int64_t num_entries = zip_get_num_entries(archive, 0);
    std::string exePath;

    for (zip_int64_t i = 0; i < num_entries; i++) {
        if (zip_stat_index(archive, i, 0, &st) == 0) {
            
            std::string fullPath = tempDir + st.name;
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
        } else {
            std::cerr << "Failed to create process. Error code: " << GetLastError() << std::endl;
        }
    } else {
        std::cerr << "Target executable '" << exeName << "' not found in zip file" << std::endl;
    }
    fs::remove_all(tempDir);    
    return true;
}

int main() {
    const std::string zipFile = "game.dat";  // Change this to your zip file name
    const std::string password = "your_password_here";  // Change this to your zip password
    const std::string exeName = "HappyStealingWithMarisaKirisame.exe";  // Change this to your game's exe name
    const std::string gameName = "Happy Stealing With Marisa Kirisame"; // Change this to your game's name
        
    if (!extractAndRun(zipFile, password, exeName)) {
        return 1;
    }
    
    std::cin.get();
    return 0;
} 