
#include <tfs/tfs.h>

#include <stdio.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* clang-format off */
#define NOT_IN_USE       ((uint8_t)0x00)
#define IN_USE           ((uint8_t)0x01)
#define IN_USE_LARG      ((uint8_t)0x81)

#define FS_TABLE_DIRTY   ((uint8_t)0x01)
/* clang-format on */

#if NUMBER_FE_RAM_CHIPS > 8
#error Only supports up to 8 feRam chips
#endif

#if NUMBER_FE_RAM_CHIPS < 1
#error Number of feRam chips must be 1 or more
#endif

static int updateFAT(FS* fs);
static void deleteFileInFAT(FS* fs, const int fd);


FS fsInt[NUMBER_FE_RAM_CHIPS];
DeviceIO_t fsDevInt[NUMBER_FE_RAM_CHIPS];

FS* tfs_getFsTablaPointer(const uint8_t id) {
    return &fsInt[id];
}

DeviceIO_t* tfs_getDevicePointer(const uint8_t id) {
    return &fsDevInt[id];
}

static int updateFAT(FS* fs) {
    if ((fs->tableFlag & FS_TABLE_DIRTY) != FS_TABLE_DIRTY) {
        return 0;
    }

    DeviceIO_t* dev = tfs_getDevicePointer(fs->devId);

    fs->tableFlag = (uint8_t)(fs->tableFlag & ~((uint8_t)FS_TABLE_DIRTY));

    return dev->io_write(0, (uint8_t*)fs, sizeof(FS));
}

static uint16_t getCapasity(FS* fs, const uint8_t fd) {
    if (fd == MAX_NO_FILES - 1) {
        return FRAM_SIZE - fs->item[fd].begin;
    }

    return fs->item[fd+1].begin - fs->item[fd].begin;
}

static void deleteFileInFAT(FS* fs, const int fd) {

    if ((fs->item[fd].flag & IN_USE_LARG) == IN_USE_LARG) {
        for (int i = 0; i < MAX_NO_FILES; i++) {
            if (fs->item[i].fd != fd) {
                continue;
            }

            fs->files[i].name[0] = '\0';
            fs->item[i].flag = NOT_IN_USE;
            fs->item[i].size = 0x00;
            fs->item[i].fd = (uint8_t)i;
        }
    }

    fs->files[fd].name[0] = '\0';
    fs->item[fd].flag = NOT_IN_USE;
    fs->item[fd].size = 0x00;
    fs->item[fd].fd = (uint8_t)fd;

    fs->tableFlag = FS_TABLE_DIRTY;
    fs->numberOfFiles--;
}

static int findFileInFAT(const FS* fs, const char* filename, int* fd) {
    for (int i = 0; i < MAX_NO_FILES; i++) {
        if ((fs->item[i].flag & IN_USE) != IN_USE) {
            continue;
        }

        // Skip if part of a large file
        if (fs->item[i].fd != i) {
            continue;
        }

        int ok = strcmp(filename, fs->files[i].name);
        if (ok == 0) {
            *fd = fs->item[i].fd;
            return 0;
        }
    }

    return -1;
}

static int findNewFATentry(FS* fs, const size_t dataSize, int* fd) {
    int firstFound = 0;
    int rc = -1;
    size_t tmp = 0;

    for (int i = 0; i < MAX_NO_FILES; i++) {
        if ((fs->item[i].flag & IN_USE) == IN_USE) {
            continue;
        }

        if (firstFound == 0) {
            firstFound = 1;
            *fd = i;
        }

        tmp += getCapasity(fs, (uint8_t)i);
        if (tmp >= dataSize) {
            rc = 0;
            break;
        }
    }

    if (rc != 0) {
        *fd = -1;
        rc = -6;
    }

    return rc;
}

FS* tfs_mount(const DeviceIO_t* devIn) {
    if (devIn == NULL) {
        return NULL;
    }

    /* Using devIn, might look a bit strange, but is it
     * done this way to force user to init the device before
     * calling the tfsMount function */
    const uint8_t id = devIn->chipNumber;

    DeviceIO_t* dev = tfs_getDevicePointer(id);

    dev->io_init();

    FS* fs = tfs_getFsTablaPointer(id);

    /* Read FAT table */
    dev->io_read(0, (uint8_t*)fs, sizeof(FS));

    fs->devId = id;

    if (fs->magic != 0xDEAD) {
        fs->err = ERR_NO_FAT_FOUND;
    }

    return fs;
}

void tfs_unMount(FS* fs) {
    DeviceIO_t* dev = tfs_getDevicePointer(fs->devId);

    /* Force write of FAT to device */
    fs->tableFlag |= FS_TABLE_DIRTY;

    (void)updateFAT(fs);

    dev->io_uninit();
}

int tfs_format(FS* fs) {
    // Note: The devId (fs->devId) is set by the
    // mount function so do not set it here

    fs->magic = 0xDEAD;
    fs->err = 0x00;
    fs->tableFlag |= FS_TABLE_DIRTY;
    fs->numberOfFiles = 0;

    const uint16_t tmp = (uint16_t)FRAM_SIZE / MAX_NO_FILES;

    for (uint16_t i = 0; i < MAX_NO_FILES; i++) {
        fs->item[i].fd = (uint8_t)i;  // Note: fd is used as an index to a array
        fs->item[i].flag = 0x00;
        fs->item[i].begin = i * tmp;
        fs->item[i].size = 0x00;
        for (int j = 0; j < FILENAME_SIZE; j++) {
            fs->files[i].name[j] = '\0';
        }
    }

    fs->item[0].begin = sizeof(FS);

    return updateFAT(fs);
}

int tfs_write(FS* fs, char* filename, uint8_t* data, const size_t dataSize) {

    int fd = -1;
    int rc = findFileInFAT(fs, filename, &fd);

    // if file found, then delete it from the FAT
    if (rc == 0) {
        deleteFileInFAT(fs, fd);
    }

    fd = -1;
    rc = findNewFATentry(fs, dataSize, &fd);
    if (rc) {
        /* Could not find space for new file */
        return rc;
    }

    DeviceIO_t* dev = tfs_getDevicePointer(fs->devId);

    if (dataSize < getCapasity(fs, (uint8_t)fd)) {
        //  Create a new file in FAT
        fs->item[fd].flag |= IN_USE;

        (void)strncpy(fs->files[fd].name, filename, FILENAME_SIZE);
        fs->item[fd].size = (uint16_t)dataSize;
        fs->item[fd].fd = (uint8_t)fd;

        rc = dev->io_write(fs->item[fd].begin, data, dataSize);
    } else {
        size_t offset = 0;
        size_t total = 0;
        for (int i = fd; i < MAX_NO_FILES; i++) {
            if ((fs->item[i].flag & IN_USE) == IN_USE) {
                continue;
            }

            if ((fs->item[i].flag & IN_USE_LARG) == IN_USE_LARG) {
                continue;
            }

            //  Create a new file in FAT
            fs->item[i].fd = (uint8_t)fd;
            fs->item[i].flag |= (IN_USE | IN_USE_LARG);

            if (i == fd) {
                (void)strncpy(fs->files[fd].name, filename, FILENAME_SIZE);
            }

            uint16_t blockCapasity = getCapasity(fs, (uint8_t)i);

            size_t writeSize = blockCapasity;

            bool finalWrite = false;
            if (total + blockCapasity > dataSize) {
                writeSize = dataSize - total;
                finalWrite = true;
            }

            rc = dev->io_write(fs->item[i].begin, data + offset, writeSize);
            fs->item[i].size = (uint16_t)writeSize;

            if (finalWrite) {
                break;
            }

            total += writeSize;
            offset += writeSize;
        }
    }

    fs->numberOfFiles++;
    fs->tableFlag |= FS_TABLE_DIRTY;

    return updateFAT(fs);
}

int tfs_read(FS* fs, char* filename, uint8_t* data, const size_t dataSize) {
    int fd = -1;
    int rc = findFileInFAT(fs, filename, &fd);
    if (rc != 0) {
        return ERR_FILE_NOT_FOUNT;
    }

    if (fs->item[fd].size > dataSize) {
        return ERR_INPUT_BUF_TOO_SMALL;
    }

    DeviceIO_t* dev = tfs_getDevicePointer(fs->devId);

    if ((fs->item[fd].flag & IN_USE_LARG) != IN_USE_LARG) {
        int ok = dev->io_read(fs->item[fd].begin, data, fs->item[fd].size);
        if (ok != 0) {
            return ERR_READ_FAILED;
        }
    } else {
        int fd0 = fs->item[fd].fd;

        int offset = 0;
        for (int i = fd; i < MAX_NO_FILES; i++) {
            if ((fs->item[i].flag & IN_USE) != IN_USE) {
                continue;
            }

            if (fs->item[i].fd != fd0) {
                continue;
            }

            int ok = dev->io_read(fs->item[i].begin, data + offset, fs->item[i].size);
            if (ok != 0) {
                return ERR_READ_FAILED;
            }

            offset += fs->item[i].size;
        }
    }

    return rc;
}

int tfs_delete(FS* fs, const char* filename) {
    int fd = -1;
    int rc = findFileInFAT(fs, filename, &fd);
    if (rc == 0) {
        deleteFileInFAT(fs, fd);
    }

    return updateFAT(fs);
}

int tfs_countFiles(const FS* fs) {
    int count = 0;
    for (int i = 0; i < MAX_NO_FILES; i++) {
        if ((fs->item[i].flag & IN_USE) != IN_USE) {
            continue;
        }

        if (fs->item[i].fd != i) {
            continue;
        }

        count++;
    }

    return count;
}

size_t tfs_getFileSize(const FS* fs, const char* filename) {
    size_t size = 0;
    int fd = -1;
    int rc = findFileInFAT(fs, filename, &fd);
    if (!rc) {
        for (int i = fd; i < MAX_NO_FILES; i++) {
            if (fs->item[i].fd != fd) {
                continue;
            }
            size += fs->item[i].size;
        }
    }

    return size;
}

#ifdef HAS_PRINTF

void tfs_printFAT(FS* fs) {
    printf("FS table\n");
    for (int i = 0; i < MAX_NO_FILES; i++) {
        Item itm = fs->item[i];
        uint16_t end = (i + 1 == MAX_NO_FILES) ? (uint16_t)FRAM_SIZE : fs->item[i + 1].begin;

        printf("%02d %02d 0x%04X 0x%04X 0x%02X size=0x%04X :%-10s\n",  //
               i,                                                      //
               itm.fd,                                                 //
               itm.begin,                                              //
               end,                                                    //
               itm.flag,                                               //
               itm.size,                                               //
               fs->files[i].name                                       //
        );
    }
}

void tfs_ls(const FS* fs) {
    for (int i = 0; i < MAX_NO_FILES; i++) {
        Item itm = fs->item[i];

        if ((itm.flag & IN_USE) != IN_USE) {
            continue;
        }

        printf("%-10s %08d\n", fs->files[i].name, itm.size);
    }
}

#endif
