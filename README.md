# yapOS

An OS simulator project to help me learn about the shell interface and basic OS functionalities like process management, memory management, and file system operations.

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
