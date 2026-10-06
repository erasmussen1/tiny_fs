# Tiny File System for FeRAM

This file system is suitable for small memory devices (character based) e.g. 32k bytes

---
Read the **[User Guide](doc/user_guide.md)** to learn more!
---

tfs is a small C library for storing persistent, named blocks of data in
ferroelectric RAM (FRAM). It is intended for bare-metal micro-controllers that
need to save small amount of data e.g. configuration, calibrations, counters, or
other small binary objects without a full operating-system file system.

The library keeps a compact file table at the start of FRAM and accesses the
device through application-provided initialization, shutdown, read, and write
callbacks.

Files form a single flat namespace. An application can format and mount the storage,
create or overwrite a file, read it, delete it, and unmount the device.

The stored data is untyped, so callers can save either raw bytes or
serialized application structures.

The current defaults target one 32 KiB FRAM device and have been exercised with
an STM32F411 (Black Pill) and an Adafruit MB85RC256V breakout.

## Build and run test on hosts

```
  ./scripts/build.sh -dceGs

```

## Contents
tfs:
File system source and header file

Examples: Example of STM32 application that use TFeRam and the appropriate I2C drivers to maintain an FRAM file system


## Capabilities

- Uses a small, allocation-free C implementation suitable for bare-metal code.
- Supports formatting, mounting, unmounting, writing, reading, and deleting
  named binary data.
- Persists the file table in FRAM and updates it when files are created or
  deleted.
- Separates the storage algorithm from the hardware driver through four device
  callbacks: `fe_init`, `fe_uninit`, `fe_read`, and `fe_write`.
- Can be configured at compile time for different FRAM sizes, file counts, and
  filename sizes by changing the constants in `tferamfs.h`.


## Limitations

tfs provides a minimal subset of normal file-system behavior.
In the current implementation:

- Storage geometry is fixed at compile time. The defaults are one 32 KiB device
  (`FRAM_SIZE`) and at most 16 files (`MAX_NO_FILES`). Although the source has a
  compile-time ceiling of eight devices, the default configuration allocates
  state for only one.

- Each file is assigned one fixed region during formatting. With the defaults,
  that is roughly 2 KiB per file, with slightly less room in the first region
  because it also contains the file table.
  A file cannot grow into unused space in a neighboring region.

- Max length of a filename is ten-byte.

- The read API does not report the stored file size.
  The caller must already know the expected size and supply a correctly sized
  destination buffer.

- The on-device file table is the in-memory `FS` structure written directly
  to the FeRAM.

- There is no checksum or thread/interrupt safety. A reset during a write
  can leave data or metadata inconsistent, and callers must serialize access.

- Device callback failures are not handled consistently by every operation.
  Hardware drivers and applications must perform their own bounds checking and
  error handling.

This design is best suited to small, controlled embedded applications with a
known set of records.

It is not intended as a replacement for a general-purpose or fault-tolerant file system.

## Usage

Add the file tfs.h and tfs.c to your micro controller project.

Here is an example of how the tfs is used.
```

int main() {

    DeviceIO_t* dev = tfsGetDevicePointer(chipNumber);
    dev->chipNumber = chipNumber;
    dev->tfs_init = framInit;
    dev->tfs_uninit = framUnInit;
    dev->tfs_read = framRead;
    dev->tfs_write = framWrite;

    FS* fs = tfsMount(dev);
    if (fs == NULL) {
        goto label_err;
    }

    if (fs->err == ERR_NO_FAT_FOUND) {
        printf("Create a FAT table on the FeRam \n");
        rc = tfs_format(fs);
        if (rc) {
            printf("ERR: Failed to create FAT for the file system (%d)\n", rc);
            goto label_err;
        }
    }



```

## Directory structure

```
.
├── app
│   ├── CMakeLists.txt
│   └── tfs
│       ├── CMakeLists.txt
│       └── tfs_main.c    <==== this is just test code for host development
├── build.sh
├── cmake
│   ├── ProjectVersion.h.in
│   └── toolchain_clang.cmake
├── CMakeLists.txt
├── debian
│   ├── postinst
│   ├── postrm
│   ├── preinst
│   └── prerm
├── doc
│   └── user_guide.md
├── lib
│   ├── CMakeLists.txt
│   └── tfs           <==== This is the libray for the tiny file system
│       ├── CMakeLists.txt
│       ├── inc
│       │   └── tfs
│       │       └── tfs.h
│       ├── src
│       │   └── tfs.c
│       └── test
│           ├── CMakeLists.txt
│           └── test_tfs.cpp
├── LICENSE
├── README.md
└── scripts
    └── build.sh

```

## License

This project is available under the [MIT License](LICENSE).
