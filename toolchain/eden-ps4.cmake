# PS4 toolchain for Eden. Uses the relocatable orbis-sdk-v1 bundle and swaps its
# libc++ for the LLVM 18 libc++ built specifically for this port.
get_filename_component(EDEN_PS4_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
include("${EDEN_PS4_ROOT}/sdk-dl/orbis-sdk-v1/toolchain/orbis-sdk.cmake")

set(EDEN_PS4_LIBCXX "${EDEN_PS4_ROOT}/libcxx18")
set(EDEN_PS4_OVERLAY "${EDEN_PS4_ROOT}/toolchain/include-overlay")

if(NOT EXISTS "${EDEN_PS4_LIBCXX}/include/c++/v1")
  message(FATAL_ERROR "Missing ${EDEN_PS4_LIBCXX}; build libc++18 first (see docs/BUILDING.md).")
endif()

string(REPLACE "-isystem ${OO_PS4_TOOLCHAIN}/include/c++/v1"
       "-nostdinc++ -fexperimental-library -isystem ${EDEN_PS4_LIBCXX}/include/c++/v1 -isystem ${EDEN_PS4_OVERLAY}"
       CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT}")
string(REPLACE "-isystem ${ORBIS_COMPAT_DIR}/include"
       "-isystem ${EDEN_PS4_OVERLAY} -isystem ${ORBIS_COMPAT_DIR}/include"
       CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")
if(NOT CMAKE_CXX_FLAGS_INIT MATCHES "libcxx18")
  message(FATAL_ERROR "eden-ps4.cmake: could not swap the SDK libc++ include for libcxx18")
endif()

string(REPLACE "-L${OO_PS4_TOOLCHAIN}/lib" "-L${EDEN_PS4_LIBCXX}/lib -L${OO_PS4_TOOLCHAIN}/lib"
       CMAKE_EXE_LINKER_FLAGS_INIT "${CMAKE_EXE_LINKER_FLAGS_INIT}")
set(CMAKE_C_STANDARD_LIBRARIES   "-lc++experimental -lc++ -lc++abi -lunwind -lc -lkernel ${ORBIS_CRT1}")
set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_C_STANDARD_LIBRARIES}")

set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} -U__FreeBSD__ -fno-omit-frame-pointer -femulated-tls -march=btver2")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} -U__FreeBSD__ -fno-omit-frame-pointer -femulated-tls -march=btver2")
