set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TOOLCHAIN_PREFIX x86_64-w64-mingw32)

set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_RC_COMPILER ${TOOLCHAIN_PREFIX}-windres)

set(CMAKE_SYSROOT /home/sk/junk/mingw-builds/install/)
set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# Never search host paths
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)


# Point to the cross-specific pkg-config executable (e.g., arm-linux-gnueabihf-pkg-config)
set(ENV{PKG_CONFIG_EXECUTABLE} "${CMAKE_SYSROOT}/lib/pkgconfig")  # Or just rely on PATH having the cross one first

# More commonly, set the path(s) to the target's .pc files.
# Prefer PKG_CONFIG_LIBDIR to fully override defaults and avoid host contamination:
set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig:${CMAKE_SYSROOT}/usr/lib/${TARGET_TRIPLE}/pkgconfig")

# Alternatively (or additionally), use PKG_CONFIG_PATH if not using a sysroot-aware wrapper:
set(ENV{PKG_CONFIG_PATH} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig:${CMAKE_SYSROOT}/usr/lib/${TARGET_TRIPLE}/pkgconfig")

# If using a sysroot-aware pkg-config wrapper, also set:
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
