# Toolchain for building LLVM 18 libc++/libc++abi for the PS4 on Linux/macOS.
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SDK "${ROOT}/sdk-dl/orbis-sdk-v1/sdk")
set(COMPAT "${ROOT}/sdk-dl/orbis-sdk-v1/orbis-compat")
set(OVERLAY "${ROOT}/toolchain/include-overlay")

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

find_program(CMAKE_C_COMPILER NAMES clang REQUIRED)
find_program(CMAKE_CXX_COMPILER NAMES clang++ REQUIRED)
find_program(CMAKE_ASM_COMPILER NAMES clang REQUIRED)
find_program(CMAKE_AR NAMES llvm-ar REQUIRED)
find_program(CMAKE_RANLIB NAMES llvm-ranlib REQUIRED)

set(CMAKE_C_COMPILER_TARGET x86_64-pc-freebsd12-elf)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-freebsd12-elf)
set(CMAKE_ASM_COMPILER_TARGET x86_64-pc-freebsd12-elf)

set(_common "-fPIC -funwind-tables -D__PS4__ -DPS4 -DORBIS -D__ORBIS__ -D_BSD_SOURCE=1 -U__FreeBSD__ -isysroot ${SDK}")
set(CMAKE_C_FLAGS_INIT "${_common} -isystem ${OVERLAY} -isystem ${COMPAT}/include -isystem ${SDK}/include -include orbis_prefix.h")
set(CMAKE_CXX_FLAGS_INIT "${_common} -nostdinc++ -isystem ${OVERLAY} -isystem ${COMPAT}/include -isystem ${SDK}/include -include orbis_prefix.h")
set(CMAKE_ASM_FLAGS_INIT "${_common}")
