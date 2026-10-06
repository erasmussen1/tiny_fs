# `tfs_shell`

`tfs_shell` is an interactive, host-side demonstration and test application for
the Tiny File System (TFS) library.

It simulates a 32 KiB FRAM device in memory and persists the simulated device
to a file named `f_ram.bin` in the current working directory.

When the application starts, it:

1. Prints the project version.
2. Creates `f_ram.bin` with random data if the file does not already exist.
3. Loads the complete image into an in-memory FRAM emulator.
4. Mounts the TFS volume and formats it automatically if no valid file table is found.
5. Opens an interactive prompt.

On normal exit, TFS is unmounted and the in-memory FRAM contents are written
back to `f_ram.bin`. This allows files stored in the simulated device to persist
between shell sessions.

## File models

The shell works with two separate collections of files:

- **Local files** are temporary, in-memory test buffers created with `mkfile`.
  They exist only for the lifetime of the shell process and are not operating
  system files.

- **Device files** are entries stored by TFS in the simulated FRAM image. They
  persist in `f_ram.bin` after the shell exits normally.

`copyto` copies a local test buffer into TFS. `copyfrom` reads a device file
into the local collection, replacing a local file of the same name.

## Commands

| Command | Description |
| --- | --- |
| `help` | Display the built-in command summary. |
| `quit` | Unmount the file system, save `f_ram.bin`, and exit. |
| `mkfile <filename> <size> <character>` | Create a local in-memory file of `size` bytes, filled with the given character. The character may be written as `A` or `'A'`. |
| `rmfile <filename>` | Remove every local file with the specified name. |
| `files` | List local in-memory files and their sizes. |
| `copyto <filename>` | Write the named local file to the TFS device. |
| `copyfrom <filename>` | Read the named TFS device file into the local collection. |
| `getfilesize <filename>` | Print the size of a file stored on the TFS device. |
| `ls` | List files stored on the TFS device. |
| `showfat` | Print the device's file allocation table. |

The built-in help currently calls the local listing command `listfiles`, but
the command accepted by the program is `files`.

## Example session

```text
> mkfile cfg.dat 133 A
>
> files
cfg.dat 133
>
> copyto cfg.dat
> ls
> getfilesize cfg.dat
133
> copyfrom cfg.dat
>
> quit
```

Command arguments are separated by whitespace, so filenames cannot contain
spaces. `mkfile` requires a non-negative decimal size and exactly one fill
character. Most commands reject unexpected extra arguments.

## Building and running

```sh
./scripts/build.sh -dce

```

To run the shell
```
./build/app/tfs_shell/tfs_shell
```

