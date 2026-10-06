
#include <tfs/tfs.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <cstddef>

#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <print>
#include <filesystem>

namespace fs = std::filesystem;


char filenameFakeFeRam[] = "f_ram.bin";
uint8_t fakeFilesystem[FRAM_SIZE];

static int framInit() {
    // std::println("Init fake FeRam");

    FILE* fp = fopen(filenameFakeFeRam, "rb");
    if (fp == nullptr) {
        std::println("ERR: Could not open file: {}", filenameFakeFeRam);
        exit(-1);
    }

    const size_t fakeFilesystemSize = sizeof(fakeFilesystem);

    const size_t bytes_read = fread(&fakeFilesystem[0], 1, fakeFilesystemSize, fp);

    if (bytes_read != fakeFilesystemSize) {
        if (ferror(fp)) {
            std::println("ERR: Failed fread()");
        } else {
            std::println("ERR: File too small: read {} bytes  {}", bytes_read, fakeFilesystemSize);
        }

        fclose(fp);
        return -1;
    }

    fclose(fp);

    return 0;
}

static int framUnInit() {
    // std::print("UnInit fake FeRam");

    FILE* fp = fopen(filenameFakeFeRam, "wb");
    if (fp == nullptr) {
        std::print("ERR: Could not open file: {}", filenameFakeFeRam);
        exit(-1);
    }

    const size_t fakeFilesystemSize = sizeof(fakeFilesystem);

    (void)fwrite(&fakeFilesystem[0], 1, fakeFilesystemSize, fp);

    fclose(fp);

    return 0;
}

static int framRead(uint16_t address, uint8_t* data, const size_t length) {
    for (size_t i = 0; i < length; i++) {
        data[i] = fakeFilesystem[address + i];
    }

    return 0;
}

static int framWrite(uint16_t address, uint8_t* data, const size_t length) {
    for (size_t i = 0; i < length; i++) {
        fakeFilesystem[address + i] = data[i];
    }

    return 0;
}

auto randomData(std::size_t size) -> std::vector<uint8_t> {
    std::vector<uint8_t> buffer(size);

    std::random_device rd;

    for (auto& byte : buffer) {
        byte = static_cast<uint8_t>(rd());
    }

    return buffer;
}

bool generateRandomFile(const std::string& filename, std::size_t size) {
    std::vector<std::uint8_t> buffer = randomData(size);

    std::ofstream output(filename, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output.write(reinterpret_cast<const char*>(buffer.data()),
                 static_cast<std::streamsize>(buffer.size()));

    return static_cast<bool>(output);
}

static auto writeVec2File(const std::filesystem::path& filename,  //
                          const std::vector<uint8_t>& buffer      //
                          ) -> bool {
    std::ofstream file(filename, std::ios::binary);

    if (!file) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(buffer.data()),
               static_cast<std::streamsize>(buffer.size()));

    return file.good();
}

struct TfsferamFixture : public ::testing::Test {
    TfsferamFixture() = default;

    ~TfsferamFixture() = default;

    void SetUp() override {
        std::string filenameFeRam{"f_ram.bin"};

        if (!fs::exists(filenameFeRam)) {
            ::generateRandomFile(filenameFeRam, FRAM_SIZE);
        }

        dev = tfs_getDevicePointer(chipNumber);
        dev->chipNumber = chipNumber;
        dev->io_init = framInit;
        dev->io_uninit = framUnInit;
        dev->io_read = framRead;
        dev->io_write = framWrite;

        fs = tfs_mount(dev);
        if (fs == nullptr) {
            return;
        }

        if (fs->err == ERR_NO_FAT_FOUND) {
            // std::print("Create a FAT table on the FeRam");
            rc = tfs_format(fs);
            if (rc) {
                std::println("ERR: Failed to create FAT for the file system ({})", rc);
                return;
            }
        }
    }

    void TearDown() override {
        // This is just to remove all
        // files between tests
        rc = tfs_format(fs);

        tfs_unMount(fs);
    }

    const uint8_t chipNumber{0};
    int rc = 0;
    DeviceIO_t* dev = nullptr;
    FS* fs = nullptr;

    std::string data = std::string{"The quick brown fox jumps over the lazy dog"};

    const std::size_t maxDefaultSize = (FRAM_SIZE / MAX_NO_FILES);
};

static auto writeTestFile(FS* fs,
                          const std::string&& baseFilename,
                          const int numberOfFiles,
                          size_t bufferSize) -> int {
    int rc = 0;

    std::vector<uint8_t> buffer0;
    buffer0 = ::randomData(bufferSize);

    for (int i = 0; i < numberOfFiles; i++) {
        std::string filename{baseFilename + std::to_string(i)};

        rc = tfs_write(fs, filename.data(), buffer0.data(), buffer0.size());
        if (rc) {
            break;
        }
    }

    return rc;
}

TEST_F(TfsferamFixture, WriteDataAndReadItBack) {
    size_t bufferSize = 100;

    std::vector<uint8_t> buffer0;
    std::vector<uint8_t> buffer1(bufferSize);

    buffer0 = ::randomData(bufferSize);

    std::string filename{"cfg0"};

    rc = tfs_write(fs, filename.data(), buffer0.data(), buffer0.size());
    ASSERT_EQ(rc, 0);

    rc = tfs_read(fs, filename.data(), buffer1.data(), buffer1.size());
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(buffer0 == buffer1);
}

TEST_F(TfsferamFixture, WriteMinimumAndMaximumFileSizes) {
    constexpr std::size_t minimumFileSize = 1;
    std::size_t maximumFileSize = maxDefaultSize - 1;

    const std::vector<std::size_t> fileSizes{minimumFileSize,
                                             maximumFileSize,
                                             maximumFileSize / 2,
                                             maximumFileSize / 4,
                                             maximumFileSize};

    int c = 0;
    for (const std::size_t fileSize : fileSizes) {
        std::vector<uint8_t> writeBuffer = ::randomData(fileSize);
        std::vector<uint8_t> readBuffer(fileSize);

        std::string filename{"d" + std::to_string(c++) + "_" + std::to_string(fileSize)};

        rc = tfs_write(fs, filename.data(), writeBuffer.data(), writeBuffer.size());
        ASSERT_EQ(rc, 0) << "Failed to write a " << fileSize << " byte file";

        rc = tfs_read(fs, filename.data(), readBuffer.data(), readBuffer.size());
        ASSERT_EQ(rc, 0) << "Failed to read a " << fileSize << " byte file";

        EXPECT_EQ(readBuffer, writeBuffer);
    }

    // ::tfs_ls(fs);

    // ::tfs_printFAT(fs);

    EXPECT_EQ(tfs_countFiles(fs), fileSizes.size());
}

TEST_F(TfsferamFixture, Write1kData) {
    size_t bufferSize = 0x150;

    std::vector<uint8_t> buffer0;
    buffer0 = ::randomData(bufferSize);

    for (int i = 0; i < 32; i++) {
        std::string filename{"cfg" + std::to_string(i)};

        rc = tfs_write(fs, filename.data(), buffer0.data(), buffer0.size());
        ASSERT_EQ(rc, 0);
    }

    int numberOfFiles = tfs_countFiles(fs);

    EXPECT_EQ(numberOfFiles, 32);
}

TEST_F(TfsferamFixture, WriteReadToMaxBlock) {
    size_t bufferSize = maxDefaultSize;

    std::vector<uint8_t> buffer0;
    std::vector<uint8_t> buffer1(bufferSize);

    buffer0 = ::randomData(bufferSize);

    std::string filename{"cfg0"};

    rc = tfs_write(fs, filename.data(), buffer0.data(), buffer0.size());
    ASSERT_EQ(rc, 0);

    rc = tfs_read(fs, filename.data(), buffer1.data(), buffer1.size());
    ASSERT_EQ(rc, 0);

    EXPECT_TRUE(buffer0 == buffer1);
}

TEST_F(TfsferamFixture, FragmentedLargeFile) {
    // fill fs and remove 3 files
    {
        rc = writeTestFile(fs, "cfg", 32, 0x100);
        EXPECT_EQ(tfs_countFiles(fs), 32);

        for (int i = 0; i < 3; i++) {
            int j = (i + 1) * 2;
            std::string filename{"cfg" + std::to_string(j)};

            ASSERT_EQ(tfs_delete(fs, filename.data()), 0);
        }
    }

    {
        const std::size_t largeFileSize = 2 * maxDefaultSize - 2;

        std::vector<uint8_t> writeBuffer = ::randomData(largeFileSize);
        std::vector<uint8_t> readBuffer(largeFileSize);

        std::string filename{"lrg1.dat"};

        rc = tfs_write(fs, filename.data(), writeBuffer.data(), writeBuffer.size());

        // tfs_printFAT(fs);

        ASSERT_EQ(rc, 0);
    }
}

TEST_F(TfsferamFixture, WriteFourSmallfilesAnd1largerInTheMiddle) {

    rc = writeTestFile(fs, "cfgA", 4, 0x100);
    ASSERT_EQ(rc, 0);

    const std::size_t largeFileSize = 3 * maxDefaultSize - 77;
    rc = writeTestFile(fs, "cfgL", 1, largeFileSize);
    ASSERT_EQ(rc, 0);

    rc = writeTestFile(fs, "cfgB", 4, 0x100);
    ASSERT_EQ(rc, 0);

    int numberOfFiles = tfs_countFiles(fs);

    // tfs_printFAT(fs);

    EXPECT_EQ(numberOfFiles, 9);
}

TEST_F(TfsferamFixture, LargeFileWriteDataAndReadItBack) {
    size_t bufferSize = (2 * maxDefaultSize) - 0x08;

    std::vector<uint8_t> buffer0;
    std::vector<uint8_t> buffer1(bufferSize);

    buffer0 = ::randomData(bufferSize);

    std::string filename{"cfg0"};

    rc = tfs_write(fs, filename.data(), buffer0.data(), buffer0.size());
    ASSERT_EQ(rc, 0);

    rc = tfs_read(fs, filename.data(), buffer1.data(), buffer1.size());
    ASSERT_EQ(rc, 0);

    // tfs_printFAT(fs);

    // writeVec2File("data_w.bin", buffer0);
    // writeVec2File("data_r.bin", buffer1);

    EXPECT_TRUE(buffer0 == buffer1);
}

TEST_F(TfsferamFixture, FileExactlyOneBlockRoundTrips) {
    const std::size_t blockSize = FRAM_SIZE / MAX_NO_FILES;
    std::vector<uint8_t> writeBuffer = ::randomData(blockSize);
    std::vector<uint8_t> readBuffer(blockSize);
    std::string filename{"block.dat"};

    ASSERT_EQ(tfs_write(fs, filename.data(), writeBuffer.data(), writeBuffer.size()), 0);
    EXPECT_EQ(tfs_countFiles(fs), 1);

    ASSERT_EQ(tfs_read(fs, filename.data(), readBuffer.data(), readBuffer.size()), 0);
    EXPECT_EQ(readBuffer, writeBuffer);
}

TEST_F(TfsferamFixture, LargeFileWithPartialFinalBlockRoundTrips) {
    const std::size_t blockSize = FRAM_SIZE / MAX_NO_FILES;
    const std::size_t fileSize = blockSize + (3 * blockSize / 4);
    std::vector<uint8_t> writeBuffer = ::randomData(fileSize);
    std::vector<uint8_t> readBuffer(fileSize);
    std::string filename{"part.dat"};

    ASSERT_EQ(tfs_write(fs, filename.data(), writeBuffer.data(), writeBuffer.size()), 0);
    ASSERT_EQ(tfs_read(fs, filename.data(), readBuffer.data(), readBuffer.size()), 0);
    EXPECT_EQ(readBuffer, writeBuffer);
}

TEST_F(TfsferamFixture, LargeFileReportsItsCompleteSize) {
    const std::size_t blockSize = FRAM_SIZE / MAX_NO_FILES;
    const std::size_t fileSize = blockSize + (blockSize / 2);
    std::vector<uint8_t> writeBuffer = ::randomData(fileSize);
    std::string filename{"size.dat"};

    ASSERT_EQ(tfs_write(fs, filename.data(), writeBuffer.data(), writeBuffer.size()), 0);
    EXPECT_EQ(tfs_getFileSize(fs, filename.data()), fileSize);
}

TEST_F(TfsferamFixture, LargeFileUsesTwoBlocksAndReadsBack) {

    const std::size_t largeFileSize = maxDefaultSize + maxDefaultSize / 2;

    std::vector<uint8_t> writeBuffer = ::randomData(largeFileSize);
    std::vector<uint8_t> readBuffer(largeFileSize);

    std::string filename{"lrg1.dat"};

    ASSERT_EQ(tfs_write(fs, filename.data(), writeBuffer.data(), writeBuffer.size()), 0);

    EXPECT_EQ(tfs_countFiles(fs), 1);

    EXPECT_EQ(tfs_getFileSize(fs, filename.data()), largeFileSize);

    ASSERT_EQ(tfs_read(fs, filename.data(), readBuffer.data(), readBuffer.size()), 0);

    EXPECT_EQ(readBuffer, writeBuffer);

    ASSERT_EQ(tfs_delete(fs, filename.data()), 0);

    EXPECT_EQ(tfs_countFiles(fs), 0);


    std::string replacement{"lrg2.dat"};
    ASSERT_EQ(tfs_write(fs, replacement.data(), writeBuffer.data(), writeBuffer.size()), 0);

    EXPECT_EQ(tfs_countFiles(fs), 1);

    {
        rc = writeTestFile(fs, "cfgA", 1, 0x100);
        rc = writeTestFile(fs, "cfgB", 1, 0x100);
        rc = writeTestFile(fs, "cfgC", 1, 0x100);
        rc = writeTestFile(fs, "cfgD", 1, 0x100);

        ASSERT_EQ(tfs_delete(fs, replacement.data()), 0);

        rc = writeTestFile(fs, "cfgE", 1, 0x100);

        EXPECT_EQ(tfs_countFiles(fs), 5);

        // tfs_printFAT(fs);
    }
}
