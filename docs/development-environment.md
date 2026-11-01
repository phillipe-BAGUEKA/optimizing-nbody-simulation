# Development environment

Environment verified on 1 November 2026.

## Machine

- Operating system: Microsoft Windows 11 Home, 64-bit, version 10.0.26200.
- Processor: Intel Core 7 150U.
- Physical cores: 10.
- Logical processors: 12.

## Toolchain

- Git 2.47.0.windows.2: `C:\Program Files\Git\cmd\git.exe`.
- GCC/G++ 16.2.0: `C:\msys64\ucrt64\bin`.
- OpenMP 5.2, with `_OPENMP` equal to `202111`.
- CMake 4.4.4: `C:\msys64\ucrt64\bin\cmake.exe`.
- Ninja 1.13.2: `C:\msys64\ucrt64\bin\ninja.exe`.
- GDB 18.1: `C:\msys64\ucrt64\bin\gdb.exe`.

The C++ build tools all come from the MSYS2 UCRT64 prefix. Git is the existing Git for Windows installation.

## Verified smoke check

Both `ucrt64-debug` and `ucrt64-release` configure and build with GCC in C++20 mode. Their CTest checks pass. The direct execution reported 12 threads in the OpenMP parallel region and produced the expected deterministic sum of `5,000,050,000`.

GDB also loaded the Debug executable, stopped at `main`, and produced a backtrace. It emitted a non-blocking warning because the MSYS2 `libstdc++` Python pretty-printer is outside GDB's configured auto-load safe path.
