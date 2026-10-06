
#include <tfs/tfs.h>

#include <vector>
#include <print>
#include <string>
#include <random>
#include <print>
#include <fstream>
#include <filesystem>
namespace fs = std::filesystem;

static auto randomData(std::size_t size) -> std::vector<uint8_t> {
    std::vector<uint8_t> buffer(size);

    std::random_device rd;

    for (auto& byte : buffer) {
        byte = static_cast<uint8_t>(rd());
    }

    return buffer;
}

static auto generateRandomFile(const std::string& filename, std::size_t size) -> bool {
    std::vector<std::uint8_t> buffer = randomData(size);

    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output.write(reinterpret_cast<const char*>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));

    return static_cast<bool>(output);
}

struct FeRAM {
    FeRAM() : fakeFilesystem(FRAM_SIZE, 0xFF) {
        //
    }

    ~FeRAM() = default;

    FeRAM(const FeRAM& other) = delete;

    FeRAM& operator=(const FeRAM& other) = delete;

    auto framInit() -> int {
        std::println("Init fake FeRam");

        FILE* fp = fopen(filename.c_str(), "rb");
        if (fp == nullptr) {
            std::println("ERR: Could not open file: {}", filename);
            exit(-1);
        }

        size_t fakeFilesystemSize = fakeFilesystem.size();

        const size_t bytes_read = fread(fakeFilesystem.data(), 1, fakeFilesystemSize, fp);

        if (bytes_read != fakeFilesystemSize) {
            if (ferror(fp)) {
                std::println(stderr, "ERR: Failed fread() ");
            } else {
                std::println(
                    "ERR: File too small: read {} bytes  {}", bytes_read, fakeFilesystemSize);
            }

            fclose(fp);
            return -1;
        }

        fclose(fp);

        return 0;
    }

    auto framUnInit() -> int {
        std::println("UnInit fake FeRam");

        FILE* fp = fopen(filename.c_str(), "wb");
        if (fp == nullptr) {
            std::println("ERR: Could not open file: {}", filename);
            exit(-1);
        }

        const size_t fakeFilesystemSize = fakeFilesystem.size();

        (void)fwrite(fakeFilesystem.data(), 1, fakeFilesystemSize, fp);

        fclose(fp);

        return 0;
    }

    auto framRead(uint16_t address, uint8_t* data, const size_t length) -> int {
        for (size_t i = 0; i < length; i++) {
            data[i] = fakeFilesystem[address + i];
        }

        return 0;
    }

    auto framWrite(uint16_t address, uint8_t* data, const size_t length) -> int {
        for (size_t i = 0; i < length; i++) {
            fakeFilesystem[address + i] = data[i];
        }

        return 0;
    }

    std::string filename{"f_ram.bin"};
    std::vector<uint8_t> fakeFilesystem;
};

inline auto generateFile() -> void {
    std::string filename{"f_ram.bin"};

    if (!fs::exists(filename)) {
        ::generateRandomFile(filename, FRAM_SIZE);
    }
}

