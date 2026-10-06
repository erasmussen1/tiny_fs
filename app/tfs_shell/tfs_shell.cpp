
#include <ProjectVersion.h>

#include <tfs/tfs.h>

#include "tfs_shell_util.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <iostream>
#include <print>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


struct Testfile {
    Testfile(std::string filename_, size_t size_, uint8_t ch_)
        : filename(std::move(filename_)), size(size_) {
        dat.resize(size_);
        std::fill(dat.begin(), dat.end(), ch_);
    }

    std::string filename;
    size_t size;
    std::vector<uint8_t> dat;
};

void mkfile(std::istringstream& input, std::vector<Testfile>& tfile) {
    std::string filename;
    std::string size_arg;
    std::string fill_arg;
    std::string extra;

    if (!(input >> filename >> size_arg >> fill_arg) || (input >> extra)) {
        std::println("usage: mkfile <filename> <size> <character>");
        return;
    }

    size_t parsed = 0;
    size_t consumed = 0;
    try {
        parsed = std::stoull(size_arg, &consumed);
    } catch (const std::exception&) {
        std::println("mkfile: invalid size '{}': expected a non-negative integer", size_arg);
        return;
    }

    if (consumed != size_arg.size() || size_arg.front() == '-') {
        std::println("mkfile: invalid size '{}': expected a non-negative integer", size_arg);
        return;
    }

    char fill = '\0';
    if (fill_arg.size() == 1) {
        fill = fill_arg[0];
    } else if (fill_arg.size() == 3 && fill_arg.front() == '\'' && fill_arg.back() == '\'') {
        fill = fill_arg[1];
    } else {
        std::println("mkfile: '{}' must be one character (for example, 'A')", fill_arg);
        return;
    }

    Testfile tmp(filename, parsed, (uint8_t)fill);
    tfile.emplace_back(tmp);
}

void rmfile(std::istringstream& input, std::vector<Testfile>& tfile) {
    std::string filename;
    std::string extra;

    if (!(input >> filename) || (input >> extra)) {
        std::println("usage: rmfile <filename>");
        return;
    }

    std::erase_if(tfile, [&filename](const Testfile& file) { return file.filename == filename; });
}

auto listfiles(std::istringstream& input, std::vector<Testfile>& tfiles) -> void {
    for (const auto& f : tfiles) {
        std::println("{} {}", f.filename, f.size);
    }
}

auto getFilesize(std::istringstream& input, FS* fs ) -> void {
    std::string filename;
    std::string extra;

    if (!(input >> filename) || (input >> extra)) {
        std::println("usage: getfilesize <filename>");
        return;
    }

    auto fileSize = tfs_getFileSize(fs, filename.data());
    std::println("{}", fileSize);
}

void copyTo(std::istringstream& input, std::vector<Testfile>& tfiles, FS* fs) {
    std::string filename;
    std::string extra;

    if (!(input >> filename) || (input >> extra)) {
        std::println("usage: copyTo <filename> ");
        return;
    }

    for (auto& f : tfiles) {
        if (filename == f.filename) {
            (void)tfs_write(fs, f.filename.data(), f.dat.data(), f.dat.size());
            return;
        }
    }
}

void copyFrom(std::istringstream& input, std::vector<Testfile>& tfiles, FS* fs) {
    std::string filename;
    std::string extra;

    if (!(input >> filename) || (input >> extra)) {
        std::println("usage: copyFrom <filename> ");
        return;
    }

    auto size = tfs_getFileSize(fs, filename.data());
    if (size == 0) {
        std::println("ERR: File does not exist!");
        return;
    }

    Testfile f(filename, size, '_');

    int rc = tfs_read(fs, f.filename.data(), f.dat.data(), f.dat.size());
    if (rc) {
        std::println("ERR: Failed to read file for device");
        return;
    }

    std::erase_if(tfiles,                                //
                  [&filename](const Testfile& file) {    //
                      return file.filename == filename;  //
                  }  //
    );

    tfiles.emplace_back(f);
}


FeRAM feRam;

auto feRamInit(void) -> int {
    return feRam.framInit();
}

auto feRamUnInit(void) -> int {
    return feRam.framUnInit();
}

auto feRamWrite(uint16_t address, uint8_t* data, const size_t length) -> int {
    return feRam.framWrite(address, data, length);
}

auto feRamRead(uint16_t address, uint8_t* data, const size_t length) -> int {
    return feRam.framRead(address, data, length);
}

const std::string helpString{ R"(

Supported commands:

  quit                                  - exit the shell
  mkfile <filename> <size> <fill char>  - create a local file
  rmfile <filename>                     - remove local file
  listfiles                             - all local files

  getfilesize <filename>                - get file size in bytes of dev fs
  ls                                    - list all files on dev fs
  showfat                               - show FAT table of dev fs
  copyto <filename>                     - copy local file to dev fs
  copyfrom <filename>                   - copy file from dev fs to local
  help                                  - show help

)"
};

int main() {
    std::println("Version: {}", versionString);

    uint8_t chipNumber = 0;

    generateFile();

    DeviceIO_t* dev = tfs_getDevicePointer(chipNumber);

    dev->io_init = feRamInit;
    dev->io_uninit = feRamUnInit;
    dev->io_read = feRamRead;
    dev->io_write = feRamWrite;

    FS* fs = tfs_mount(dev);
    if (fs == nullptr) {
        exit(-1);
    }

    int rc = 0;
    if (fs->err == ERR_NO_FAT_FOUND) {
        // std::print("Create a FAT table on the FeRam");
        rc = tfs_format(fs);
        if (rc) {
            std::println("ERR: Failed to create FAT for the file system ({})", rc);
            return -1;
        }
    }

    // This is used by the shell
    std::vector<Testfile> testFiles;

    std::string line;
    while (true) {
        std::print("> ");
        std::fflush(stdout);

        if (!std::getline(std::cin, line)) {
            break;
        }

        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command == "quit") {
            break;
        } else if (command == "mkfile") {
            mkfile(input, testFiles);
        } else if (command == "rmfile") {
            rmfile(input, testFiles);
        }else if (command == "files") {
            listfiles(input, testFiles);
        } else if (command == "getfilesize") {
            getFilesize(input, fs);
        } else if (command == "showfat") {
            tfs_printFAT(fs);
        } else if (command == "ls") {
            tfs_ls(fs);
        } else if (command == "copyto") {
            copyTo(input, testFiles, fs);
        } else if (command == "copyfrom") {
            copyFrom(input, testFiles, fs);
        } else if (command == "help") {
            std::println("{}", helpString);
        } else {
            std::println("command not found");
        }
    }

    tfs_unMount(fs);

    return 0;
}
