#pragma once

/* clang-format off */
#define NUMBER_FE_RAM_CHIPS 1

#define FRAM_SIZE          ((uint16_t)0x8000) /* 32768 */
#define FILENAME_SIZE               10
#define MAX_NO_FILES     ((uint8_t)32)

#define ERR_READ_FAILED           0x01
#define ERR_FILE_NOT_FOUNT        0x02
#define ERR_NO_FAT_FOUND          0x04
#define ERR_INPUT_BUF_TOO_SMALL   0x08


#define HAS_PRINTF

/* clang-format on */


#include <stdint.h>
#include <stddef.h>

#ifdef HAS_PRINTF
#include <stdio.h>
#endif

typedef struct {
    uint8_t fd;
    uint8_t flag;
    uint16_t size;
    uint16_t begin;
} Item;

typedef struct {
    char name[FILENAME_SIZE];
} Filename_t;

typedef struct {
    uint16_t magic;
    uint8_t err;
    uint8_t tableFlag;
    uint8_t numberOfFiles;
    uint8_t devId;
    Item item[MAX_NO_FILES];
    Filename_t files[MAX_NO_FILES+1];
} FS;

typedef struct {
    uint8_t chipNumber;
    int (*io_init)(void);
    int (*io_uninit)(void);
    int (*io_read)(uint16_t, uint8_t* , const size_t);
    int (*io_write)(uint16_t, uint8_t* , const size_t);
} DeviceIO_t;

#ifdef __cplusplus
extern "C" {
#endif

DeviceIO_t* tfs_getDevicePointer(const uint8_t id);

FS* tfs_getFsTablaPointer(const uint8_t id);

FS* tfs_mount(const DeviceIO_t* dev);

void tfs_unMount(FS* fs);

int tfs_format(FS* fs);

int tfs_write(FS* fs, char* filename, uint8_t* data, const size_t dataSize);

int tfs_read(FS* fs, char* filename, uint8_t* data, const size_t dataSize);

int tfs_delete(FS* fs, const char* filename);

int tfs_countFiles(const FS* fs);

size_t tfs_getFileSize(const FS* fs, const char* filename);

#ifdef HAS_PRINTF
void tfs_printFAT(FS* fs);

void tfs_ls(const FS* fs);
#endif

#ifdef __cplusplus
}
#endif

