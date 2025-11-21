# Linux Kernel Modules

This repository contains a set of Linux kernel modules of different levels of complexity containing various memory-related vulnerabilities. The main purpose of these modules is to test the taint tracking functionality of a prototype static analysis compiler pass implemented in LLVM.

## Recommended environment for testing

Any x86-64 Linux distribution. For testing the taint tracking functionality, the Clang compiler should be built from source with the taint analysis compiler pass. The source code for the prototype, as well as more information on how to build LLVM with the additional compiler pass, can be found [here](https://github.com/FlorinChess/llvm-project).

## Building the modules

For building the modules using build, run the following script in the root of the repository:  

```shell
./compile_modules.sh
```

If you want to let Clang generate the LLVM modules (`.ll` files) for the Linux modules, run the following script in the root of the repository:

```shell
./compile_modules_ll.sh <OUTPUT_DIRECTORY>
```

where the command line argument `OUTPUT_DIRECTORY` specifies where the compiled .ll files will be stored.
