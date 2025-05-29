# yapOS

An OS simulator project to help me learn about the shell interface and basic OS functionalities like process management, memory management, and file system operations. It demonstrates core systems programming concepts including:

- Variable storage and key-value substitution
- Round-robin process scheduling with paging
- Demand-paged memory management with LRU eviction
- A persistent, FAT-style virtual file system
- Script execution, command parsing, and a shell REPL

The design mimics a simplified OS kernel layered over a block device simulator (`blank.dsk`).

## Architecture Overview

| Layer                 | Files                         | Responsibilities                                                                         |
|-----------------------|-------------------------------|------------------------------------------------------------------------------------------|
| Shell & Interpreter   | `shell.c`, `interpreter.c`    | Tokenize user input, dispatch commands, stage scripts.                                   |
| Kernel                | `kernel.c`                    | Handle ready queue, scheduling, paging, and file operations.                             |
| CPU Emulator          | `cpu.c`                       | Fetch and execute script lines per time slice.                                           |
| PCB & Scheduling      | `pcb.c`, `linked_list.c`      | Maintain PCBs with program counters, file tables, and page metadata.                     |
| Shell Memory          | `shellmemory.c`               | Simple variable store with frame allocation.                                             |
| File System           | `blank.dsk`, `kernel.c`       | Simulate block-based file storage using a FAT and sector map.                            |

## Backing Store & Paging

When scripts are loaded via `run` or `exec`, they are copied into a temporary `backing_store/` directory.  
Each script becomes a private file used for demand paging. The system supports a small fixed number of memory frames and uses **3-line pages** with **LRU replacement**. This structure mimics real-world virtual memory backed by secondary storage.

## Requirements

- GCC compiler
- Make build system

## Building

To build the project, use the `make` command in the root directory:

```bash
make
```

This will compile all the necessary components and create the `myshell` executable.

## Running

To run yapOS, use the following command:

```bash
./myshell blank.dsk
```

The `blank.dsk` file serves as the disk image for the operating system.

## Configuration

The build process supports the following optional parameters:

- `framesize`: Size of the frame store (default: 21)
- `varmemsize`: Size of the variable memory store (default: 10)

You can specify these parameters when running make:

```bash
make framesize=30 varmemsize=15
```

## Cleaning

To clean the build artifacts:

```bash
make clean
```

## Command Reference

| Command                 | Syntax                                | Description                                                   |
|-------------------------|---------------------------------------|---------------------------------------------------------------|
| `help`                  | `help`                                | Lists all available commands.                                 |
| `quit`, `exit`          | `quit` / `exit`                       | Cleanly shuts down the shell and syncs the disk.              |
| `set`                   | `set <var> <value...>`                | Stores a variable with the given value (spaces included).     |
| `print`                 | `print <var>`                         | Prints the value of a variable.                               |
| `echo`                  | `echo <text>` / `echo $<var>`         | Prints text or a substituted variable.                        |
| `resetmem`              | `resetmem`                            | Clears all stored variables.                                  |
| `run`                   | `run <script.txt>`                    | Loads and executes a script sequentially.                     |
| `exec`                  | `exec <s1> [s2] [s3] [s4]`            | Loads and schedules multiple scripts in round-robin.          |
| `ls`                    | `ls`                                  | Lists files in the virtual root directory.                    |
| `cat`                   | `cat <file>`                          | Prints contents of a file from the beginning.                 |
| `create`                | `create <file> <bytes>`               | Creates a new file of given size.                             |
| `write`                 | `write <file> <data...>`              | Writes data at the current file pointer.                      |
| `read`                  | `read <file> <n>`                     | Reads `n` bytes from current pointer.                         |
| `seek`                  | `seek <file> <offset>`                | Sets the current pointer position.                            |
| `size`                  | `size <file>`                         | Shows the file size in bytes.                                 |
| `rm`                    | `rm <file>`                           | Deletes a file and frees its blocks.                          |
| `find_file`             | `find_file <substring...>`            | Searches for filenames containing the substring.              |
| `copy_in`               | `copy_in <host_path>`                 | Imports a file from the host system.                          |
| `copy_out`              | `copy_out <file>`                     | Exports a virtual file to the host system.                    |
| `freespace`             | `freespace`                           | Shows free sectors and total free space.                      |
| `fragmentation_degree`  | `fragmentation_degree`                | Calculates fragmentation for each file.                       |
| `defragment`            | `defragment`                          | Rewrites files to be stored contiguously.                     |
| `recover`               | `recover <0 or 1>`                    | Attempts file system repair (0 = thorough, 1 = fast).         |

*Unknown or malformed commands return standardized error messages prefixed with `Bad command:`.*
