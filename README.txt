Build Instructions
--------------------
Windows
=======
Since, the curl libraries included are compiled with MSVC, you need the same compiler on system. This means compiling using other compilers like gcc fail.

How to compile in windows?
---------------------------
1. Using Visual Studio
Open the folder( containing CMakelists.txt) in Visual studio. (Inside Visual Studio, File->Open Folder->..). Then it will be opened as a CMake Project.
There will be 4 main configurations: WINDOWS_32, WINDOWS_64, WSL_GCC_32, WSL_GCC_64. The latter two are for compiling using WSL installed. To compile windows plugins, select WINDOWS_64 or WINDOWS_32 as the case may be.
Finally, Build using Build->Build All

2. Using Command Prompt

open a command prompt and browse to the directory containing CMakelists.txt and:
mkdir out
cd out
cmake ../ -DCURL_LIBRARY=../libcurl-x64 -DCURL_INCLUDE_DIR=../curl 
cmake --build . --config Release

2. (a) For compiling 32-bit build on 64-bit windows, it will be:
mkdir out
cd out
cmake ../ -DCURL_LIBRARY=../libcurl-x86 -DCURL_INCLUDE_DIR=../curl -DCMAKE_GENERATOR_PLATFORM=Win32
cmake --build . --config Release

The outputs discordsync04rel64.dll or d**32.dll can be found on out/binaries

Linux
==================
For 64 bit systems:
In linux also you must have cmake and browse to directory containing CMakelists.txt and:
mkdir out
cd out
cmake ../ -DCURL_LIBRARY=libcurl-x64.a -DOPENSSL=libssl-x64.a -DCRYPTO=libcrypto-x64.a 
cmake --build . config --Release

This will build discordsync04rel64.so and copy it to out/binaries

If you want to build for 32 bit systems: (on 32 bit system )
mkdir out
cd out
cmake ../ -DCURL_LIBRARY=libcurl-x86.a -DOPENSSL=libssl-x86.a -DCRYPTO=libcrypto-x86.a 
cmake --build . config --Release
(it shows some warning, but it works when tested)

