# Recreating the I/O Firmware v2.0 Binary

This document explains how to recreate the exact same binary file as the [main branch](https://github.com/ReflexCreations/io-firmware)'s I/O Firmware v2.0 release. 

The steps below are necessary to compile the exact same binary file.  Different BSP/Compiler combinations have different quirks.

## Prerequisites

GNU ARM Embedded Toolchain:  **9-2019-q4-major**  
* [gcc-arm-none-eabi-9-2019-q4-major-win32.zip](https://developer.arm.com/-/media/Files/downloads/gnu-rm/9-2019q4/gcc-arm-none-eabi-9-2019-q4-major-win32.zip)
* [https://developer.arm.com/downloads/-/gnu-rm](https://developer.arm.com/downloads/-/gnu-rm)

> *Note:* Starting with GCC 10, the default is ‑fno‑common, which causes a linker error.

Strawberry Perl: **5.38.0.1** Portable Edition 
* [strawberry-perl-5.38.0.1-64bit-portable.zip](https://github.com/StrawberryPerl/Perl-Dist-Strawberry/releases/download/SP_5380_5361/strawberry-perl-5.38.0.1-64bit-portable.zip)
* [https://strawberryperl.com/releases.html](https://strawberryperl.com/releases.html)  

> *Note:* This flavor of Perl includes the correct version of `mingw32-make.exe`.

## Instructions

### 1. Install the Required Toolchains

Extract GNU ARM Embedded Toolchain (9-2019-q4-major) and Strawberry Perl (5.38.0.1) to the C drive so that the `bin` folders are located at:
```
  C:\SysGCC\arm-eabi-921\bin
  C:\perl\c\bin
```

### 2. Set System Environment Paths

1. **Update Windows Environment Variables:**
   - Open **System Properties** (Start menu > search for Environment Variables).
   - Edit the `PATH` variable to include the following directories:
   
     ```
     C:\SysGCC\arm-eabi-921\bin
     C:\perl\c\bin
     ```
   - Click OK to apply the changes.

   - Open a **NEW** command prompt and run:
   
     ```bash
     where arm-none-eabi-gcc
     where mingw32-make
     ```
     > Both commands should return the correct paths.

### 3. Clone Repo and Build

* Open a command prompt.
* Clone the repository
* Switch to the v2.0 tagged release
* Make the binary
   
     ```bash
     git clone https://github.com/ReflexCreations/io-firmware.git
     cd io-firmware
     git checkout tags/v2.0
     mingw32-make -j8 all TARGET=io_firmware OPT="-O2"
     ```

## Verification

After the build completes, you can use the built‐in Windows file comparison tool in binary mode.

```bash
fc /b build\io_firmware.hex release\io_firmware.hex
```

Verify that the files are identical.  The above command should report "no differences encountered." 
