# toolchain-mingw64.cmake — Cross-compilazione Windows x86_64 da Linux
# Uso: cmake -B build_cross_win gui/ -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake
#           -DCMAKE_PREFIX_PATH=<percorso Qt6 mingw>
#
# Installa compilatore: sudo apt install mingw-w64

set(CMAKE_SYSTEM_NAME    Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER     x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER   x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER    x86_64-w64-mingw32-windres)

# Cerca librerie e header solo nel sysroot cross, mai nel sistema Linux
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
