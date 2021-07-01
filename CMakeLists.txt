cmake_minimum_required(VERSION 3.4)

project(LibVNCServer VERSION 0.9.13 LANGUAGES C)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckIncludeFile)
include(CheckTypeSize)
include(TestBigEndian)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(GNUInstallDirs)

enable_testing()

set(PACKAGE_NAME           "LibVNCServer")
set(FULL_PACKAGE_NAME      "LibVNCServer")
set(VERSION_SO             "1")
set(PROJECT_BUGREPORT_PATH "https://github.com/LibVNC/libvncserver/issues")
set(LIBVNCSERVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libvncserver)
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(LIBVNCCLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libvncclient)
set(LIBVNCSRVEXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(LIBVNCCLIEXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/client_examples)
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")
    # some LSP servers expect compile_commands.json in the project root
    add_custom_target(
        copy-compile-commands ALL
        ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}
        )
endif(CMAKE_GENERATOR MATCHES "Unix Makefiles|Ninja")

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/libvncserver ${CMAKE_CURRENT_SOURCE_DIR}/common)

# all the build configuration switches
option(LIBVNCSERVER_INSTALL "Generate installation target" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ${UNIX})
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(WITH_ZLIB "Search for the zlib compression library to support additional encodings" ON)
option(WITH_LZO "Search for the LZO compression library to omit internal miniLZO implementation" ON)
option(WITH_JPEG "Search for the libjpeg compression library to support additional encodings" ON)
option(WITH_PNG "Search for the PNG compression library to support additional encodings" ON)
option(WITH_SDL "Search for the Simple Direct Media Layer library to build an example SDL vnc client" ON)
option(WITH_GTK "Search for the GTK library to build an example GTK vnc client" ON)
option(WITH_THREADS "Search for a threading library to build with multithreading support" ON)
option(PREFER_WIN32THREADS "When searching for a threading library, prefer win32 threads if they are found" OFF)
option(WITH_GNUTLS "Search for the GnuTLS secure communications library to support TLS" ON)
option(WITH_OPENSSL "Search for the OpenSSL cryptography library to support TLS and use as crypto backend" ON)
option(WITH_SYSTEMD "Search for libsystemd to build with systemd socket activation support" ON)
option(WITH_GCRYPT "Search for Libgcrypt to use as crypto backend" ON)
option(WITH_FFMPEG "Search for FFMPEG to build an example VNC to MPEG encoder" ON)
option(WITH_TIGHTVNC_FILETRANSFER "Enable filetransfer if there is pthreads support" ON)
option(WITH_24BPP "Allow 24 bpp" ON)
option(WITH_IPv6 "Enable IPv6 Support" ON)
option(WITH_WEBSOCKETS "Build with websockets support" ON)
option(WITH_SASL "Build with SASL support" ON)

if(WITH_ZLIB)
  find_package(ZLIB)
endif(WITH_ZLIB)

if(WITH_LZO)
  find_package(LZO)
endif()

if(WITH_JPEG)
  find_package(JPEG)
  if(JPEG_FOUND)
    # Check whether the version of libjpeg we found was libjpeg-turbo and print a
    # warning if not.
    set(CMAKE_REQUIRED_LIBRARIES ${JPEG_LIBRARIES})
    if(JPEG_INCLUDE_DIRS) # this was not present in 3.4
      set(CMAKE_REQUIRED_INCLUDES ${JPEG_INCLUDE_DIRS})
    else()
      set(CMAKE_REQUIRED_INCLUDES ${JPEG_INCLUDE_DIR})
    endif()

    set(JPEG_TEST_SOURCE "\n
    #include <stdio.h>\n
    #include <jpeglib.h>\n
    int main(void) {\n
      struct jpeg_compress_struct cinfo;\n
      struct jpeg_error_mgr jerr;\n
      cinfo.err=jpeg_std_error(&jerr);\n
      jpeg_create_compress(&cinfo);\n
      cinfo.input_components = 3;\n
      jpeg_set_defaults(&cinfo);\n
      cinfo.in_color_space = JCS_EXT_RGB;\n
      jpeg_default_colorspace(&cinfo);\n
      return 0;\n
    }")

    if(CMAKE_CROSSCOMPILING)
      check_c_source_compiles("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
    else()
      check_c_source_runs("${JPEG_TEST_SOURCE}" FOUND_LIBJPEG_TURBO)
    endif()

    set(CMAKE_REQUIRED_LIBRARIES)
    set(CMAKE_REQUIRED_INCLUDES)
    set(CMAKE_REQUIRED_DEFINITIONS)

    if(NOT FOUND_LIBJPEG_TURBO)
        # last-resort grep check (when using LibVNCServer as a CMake subdir together with turbojpeg CMake subdir, the build check above fails since turbojpeg is not yet built when LibVNCServer is configured)
        get_filename_component(JPEGLIB_H_PATH "${JPEG_INCLUDE_DIR}/jpeglib.h" REALPATH BASE_DIR "${CMAKE_SOURCE_DIR}")
        message(STATUS "Runtime check for libjpeg-turbo failed, inspecting ${JPEGLIB_H_PATH}")
        file(STRINGS ${JPEGLIB_H_PATH} FOUND_LIBJPEG_TURBO REGEX "JCS_EXT_RGB")
        if(NOT FOUND_LIBJPEG_TURBO)
            message(WARNING "*** The libjpeg library you are building against is not libjpeg-turbo.  Performance will be reduced.  You can obtain libjpeg-turbo from:  https://sourceforge.net/projects/libjpeg-turbo/files/ ***")
        endif()
    endif()
endif(JPEG_FOUND)
endif(WITH_JPEG)


if(WITH_PNG)
  find_package(PNG)
endif(WITH_PNG)


if(WITH_SDL)
  find_package(SDL2)
endif(WITH_SDL)

if(WITH_GTK)
  find_package(GTK2)
endif(WITH_GTK)

if(WITH_THREADS)
  find_package(Threads)
endif(WITH_THREADS)


if(WITH_GNUTLS)
  find_package(GnuTLS)
endif(WITH_GNUTLS)


if(WITH_OPENSSL)
  find_package(OpenSSL)
endif(WITH_OPENSSL)


if(WITH_SYSTEMD AND NOT ANDROID AND NOT WIN32)
  find_package(PkgConfig)
  pkg_check_modules(SYSTEMD "libsystemd")
endif(WITH_SYSTEMD AND NOT ANDROID AND NOT WIN32)


if(WITH_GCRYPT)
  find_library(LIBGCRYPT_LIBRARIES gcrypt)
endif(WITH_GCRYPT)

if(WITH_FFMPEG)
  find_package(FFMPEG 3.1.0 COMPONENTS avformat avcodec avutil swscale)
endif(WITH_FFMPEG)


check_include_file("dirent.h"      LIBVNCSERVER_HAVE_DIRENT_H)
check_include_file("endian.h"      LIBVNCSERVER_HAVE_ENDIAN_H)
check_include_file("fcntl.h"       LIBVNCSERVER_HAVE_FCNTL_H)
check_include_file("netinet/in.h"  LIBVNCSERVER_HAVE_NETINET_IN_H)
check_include_file("sys/endian.h"  LIBVNCSERVER_HAVE_SYS_ENDIAN_H)
check_include_file("sys/socket.h"  LIBVNCSERVER_HAVE_SYS_SOCKET_H)
check_include_file("sys/stat.h"    LIBVNCSERVER_HAVE_SYS_STAT_H)
check_include_file("sys/time.h"    LIBVNCSERVER_HAVE_SYS_TIME_H)
check_include_file("sys/types.h"   LIBVNCSERVER_HAVE_SYS_TYPES_H)
check_include_file("sys/wait.h"    LIBVNCSERVER_HAVE_SYS_WAIT_H)
check_include_file("unistd.h"      LIBVNCSERVER_HAVE_UNISTD_H)
check_include_file("sys/resource.h"     LIBVNCSERVER_HAVE_SYS_RESOURCE_H)


# headers needed for check_type_size()
check_include_file("vfork.h"       LIBVNCSERVER_HAVE_VFORK_H)
check_include_file("ws2tcpip.h"    LIBVNCSERVER_HAVE_WS2TCPIP_H)
check_include_file("arpa/inet.h"   HAVE_ARPA_INET_H)
check_include_file("stdint.h"      HAVE_STDINT_H)
check_include_file("stddef.h"      HAVE_STDDEF_H)
check_include_file("sys/types.h"   HAVE_SYS_TYPES_H)

# error out if required headers not found
if(NOT HAVE_STDINT_H)
  message(FATAL_ERROR "Could NOT find required header stdint.h")
endif()

check_function_exists(gettimeofday    LIBVNCSERVER_HAVE_GETTIMEOFDAY)
check_function_exists(vfork           LIBVNCSERVER_HAVE_VFORK)
check_function_exists(vprintf         LIBVNCSERVER_HAVE_VPRINTF)
check_function_exists(mmap            LIBVNCSERVER_HAVE_MMAP)
check_function_exists(fork            LIBVNCSERVER_HAVE_FORK)
check_function_exists(ftime           LIBVNCSERVER_HAVE_FTIME)
check_function_exists(gethostbyname   LIBVNCSERVER_HAVE_GETHOSTBYNAME)
check_function_exists(gethostname     LIBVNCSERVER_HAVE_GETHOSTNAME)
check_function_exists(inet_ntoa       LIBVNCSERVER_HAVE_INET_NTOA)
check_function_exists(memmove         LIBVNCSERVER_HAVE_MEMMOVE)
check_function_exists(memset          LIBVNCSERVER_HAVE_MEMSET)
check_function_exists(mkfifo          LIBVNCSERVER_HAVE_MKFIFO)
check_function_exists(select          LIBVNCSERVER_HAVE_SELECT)
check_function_exists(socket          LIBVNCSERVER_HAVE_SOCKET)
check_function_exists(strchr          LIBVNCSERVER_HAVE_STRCHR)
check_function_exists(strcspn         LIBVNCSERVER_HAVE_STRCSPN)
check_function_exists(strdup          LIBVNCSERVER_HAVE_STRDUP)
check_function_exists(strerror        LIBVNCSERVER_HAVE_STRERROR)
check_function_exists(strstr          LIBVNCSERVER_HAVE_STRSTR)

check_symbol_exists(htobe64 "endian.h" LIBVNCSERVER_HAVE_HTOBE64)
check_symbol_exists(OSSwapHostToBigInt64 "libkern/OSByteOrder.h" LIBVNCSERVER_HAVE_OSSWAPHOSTTOBIGINT64)

if(WITH_THREADS AND Threads_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} Threads")
    list(APPEND COMMON_LIBS Threads::Threads)
    list(APPEND PC_COMMON_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif(WITH_THREADS AND Threads_FOUND)
if(SYSTEMD_FOUND)
    list(APPEND COMMON_DEFINES LIBVNCSERVER_WITH_SYSTEMD)
    include_directories(${SYSTEMD_INCLUDE_DIRS})
    list(APPEND PC_COMMON_LIBS ${SYSTEMD_LIBRARIES})
    list(APPEND COMMON_LIBS ${SYSTEMD_LIBRARIES})
endif(SYSTEMD_FOUND)

# common crypto used by both libvncserver and libvncclient
if(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)
    message(STATUS "Building crypto with Libgcrypt")
    list(APPEND COMMON_LIBS ${LIBGCRYPT_LIBRARIES})
    list(APPEND PC_COMMON_LIBS ${LIBGCRYPT_LIBRARIES})
    set(CRYPTO_SOURCES ${COMMON_DIR}/crypto_libgcrypt)
elseif(OPENSSL_FOUND)
    message(STATUS "Building crypto with OpenSSL")
    list(APPEND COMMON_LIBS OpenSSL::Crypto)
    list(APPEND PC_COMMON_LIBS ${OPENSSL_CRYPTO_LIBRARIES})
    set(CRYPTO_SOURCES ${COMMON_DIR}/crypto_openssl)
else()
    message(STATUS "Building crypto with builtin functions, only including SHA1 and D3DES")
    set(CRYPTO_SOURCES ${COMMON_DIR}/crypto_included.c ${COMMON_DIR}/sha1.c ${COMMON_DIR}/d3des.c)
endif()

if(WITH_WEBSOCKETS AND (LIBVNCSERVER_HAVE_HTOBE64 OR LIBVNCSERVER_HAVE_OSSWAPHOSTTOBIGINT64))
  set(LIBVNCSERVER_WITH_WEBSOCKETS 1)
endif()

if(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)
  set(LIBVNCSERVER_HAVE_LIBGCRYPT 1)
endif(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)

if(GNUTLS_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} GnuTLS")
    list(APPEND COMMON_DEFINES LIBVNCSERVER_HAVE_GNUTLS)
endif(GNUTLS_FOUND)

if(OPENSSL_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} OpenSSL")
    list(APPEND COMMON_DEFINES LIBVNCSERVER_HAVE_LIBSSL)
endif(OPENSSL_FOUND)

if(WITH_IPv6)
  if(WIN32 AND LIBVNCSERVER_HAVE_WS2TCPIP_H AND LIBVNCSERVER_HAVE_VPRINTF)
    set(LIBVNCSERVER_IPv6 1)
  endif()
  if(NOT WIN32)
    set(LIBVNCSERVER_IPv6 1)
  endif()
endif(WITH_IPv6)


if(WITH_24BPP)
  set(LIBVNCSERVER_ALLOW24BPP 1)
endif()

# Make sure that only one threading system is used. This happens on MinGW.
if(WITH_THREADS)
  if(CMAKE_USE_PTHREADS_INIT AND CMAKE_USE_WIN32_THREADS_INIT)
    if(PREFER_WIN32THREADS)
      unset(CMAKE_USE_PTHREADS_INIT)
    else()
      unset(CMAKE_USE_WIN32_THREADS_INIT)
    endif(PREFER_WIN32THREADS)
  endif(CMAKE_USE_PTHREADS_INIT AND CMAKE_USE_WIN32_THREADS_INIT)

  if(CMAKE_USE_PTHREADS_INIT)
    list(APPEND COMMON_DEFINES LIBVNCSERVER_HAVE_LIBPTHREAD)
    message(STATUS "Threads support is using pthreads")
  endif(CMAKE_USE_PTHREADS_INIT)

  if(CMAKE_USE_WIN32_THREADS_INIT)
    list(APPEND COMMON_DEFINES LIBVNCSERVER_HAVE_WIN32THREADS)
    message(STATUS "Threads support is using win32 threads")
  endif(CMAKE_USE_WIN32_THREADS_INIT)
endif(WITH_THREADS)

if(LIBVNCSERVER_HAVE_SYS_SOCKET_H)
  # socklen_t
  list(APPEND CMAKE_EXTRA_INCLUDE_FILES "sys/socket.h")
endif(LIBVNCSERVER_HAVE_SYS_SOCKET_H)
if(HAVE_ARPA_INET_H)
  # in_addr_t
  list(APPEND CMAKE_EXTRA_INCLUDE_FILES "arpa/inet.h")
endif(HAVE_ARPA_INET_H)

check_type_size(pid_t     LIBVNCSERVER_PID_T)
check_type_size(size_t    LIBVNCSERVER_SIZE_T)
check_type_size(socklen_t LIBVNCSERVER_SOCKLEN_T)
check_type_size(in_addr_t LIBVNCSERVER_IN_ADDR_T)
if(NOT HAVE_LIBVNCSERVER_IN_ADDR_T)
  set(LIBVNCSERVER_NEED_INADDR_T 1)
endif(NOT HAVE_LIBVNCSERVER_IN_ADDR_T)

TEST_BIG_ENDIAN(LIBVNCSERVER_WORDS_BIGENDIAN)

if(WITH_SASL)
  find_path(SASL2_INCLUDE_DIR sasl/sasl.h)
  find_library(LIBSASL2_LIBRARIES sasl2 libsasl.lib)
endif(WITH_SASL)

if(WITH_SASL AND LIBSASL2_LIBRARIES AND SASL2_INCLUDE_DIR)
  message(STATUS "Building with SASL: ${LIBSASL2_LIBRARIES} and ${SASL2_INCLUDE_DIR}")
  set(LIBVNCSERVER_HAVE_SASL 1)
  list(APPEND PC_COMMON_LIBS ${LIBSASL2_LIBRARIES})
  list(APPEND COMMON_LIBS ${LIBSASL2_LIBRARIES})
  include_directories(${SASL2_INCLUDE_DIR})
endif(WITH_SASL AND LIBSASL2_LIBRARIES AND SASL2_INCLUDE_DIR)

# TODO:
# LIBVNCSERVER_ENOENT_WORKAROUND
# inline

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/rfb/rfbconfig.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/rfb/rfbconfig.h)

set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_DIR}/main.c
    ${LIBVNCSERVER_DIR}/rfbserver.c
    ${LIBVNCSERVER_DIR}/rfbregion.c
    ${LIBVNCSERVER_DIR}/auth.c
    ${LIBVNCSERVER_DIR}/sockets.c
    ${LIBVNCSERVER_DIR}/stats.c
    ${LIBVNCSERVER_DIR}/corre.c
    ${LIBVNCSERVER_DIR}/hextile.c
    ${LIBVNCSERVER_DIR}/rre.c
    ${LIBVNCSERVER_DIR}/translate.c
    ${LIBVNCSERVER_DIR}/cutpaste.c
    ${LIBVNCSERVER_DIR}/httpd.c
    ${LIBVNCSERVER_DIR}/cursor.c
    ${LIBVNCSERVER_DIR}/font.c
    ${LIBVNCSERVER_DIR}/draw.c
    ${LIBVNCSERVER_DIR}/selbox.c
    ${COMMON_DIR}/vncauth.c
    ${LIBVNCSERVER_DIR}/cargs.c
    ${LIBVNCSERVER_DIR}/ultra.c
    ${LIBVNCSERVER_DIR}/scale.c
    ${CRYPTO_SOURCES}
)

set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_DIR}/cursor.c
    ${LIBVNCCLIENT_DIR}/listen.c
    ${LIBVNCCLIENT_DIR}/rfbproto.c
    ${LIBVNCCLIENT_DIR}/sockets.c
    ${LIBVNCCLIENT_DIR}/vncviewer.c
    ${CRYPTO_SOURCES}
)

if(GNUTLS_FOUND)
    message(STATUS "Building TLS with GnuTLS")
    list(APPEND COMMON_LIBS GnuTLS::GnuTLS)
    list(APPEND PC_COMMON_LIBS ${GNUTLS_LIBRARIES})
    list(APPEND LIBVNCCLIENT_SOURCES
        ${LIBVNCCLIENT_DIR}/tls_gnutls.c
        )
    list(APPEND LIBVNCSERVER_SOURCES
        ${LIBVNCSERVER_DIR}/rfbssl_gnutls.c
        )
elseif(OPENSSL_FOUND)
    message(STATUS "Building TLS with OpenSSL")
    list(APPEND LIBVNCCLIENT_SOURCES
        ${LIBVNCCLIENT_DIR}/tls_openssl.c
        )
    list(APPEND LIBVNCSERVER_SOURCES
        ${LIBVNCSERVER_DIR}/rfbssl_openssl.c
        )
    list(APPEND COMMON_LIBS OpenSSL::SSL)
    list(APPEND PC_COMMON_LIBS ${OPENSSL_SSL_LIBRARIES})
else()
    message(STATUS "Building without TLS")
    list(APPEND LIBVNCCLIENT_SOURCES
        ${LIBVNCCLIENT_DIR}/tls_none.c
        )
    list(APPEND LIBVNCSERVER_SOURCES
        ${LIBVNCSERVER_DIR}/rfbssl_none.c
        )
endif()

if(LIBVNCSERVER_HAVE_SASL)
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/sasl.c
  )
endif()

if(ZLIB_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} ZLIB")
    list(APPEND COMMON_DEFINES LIBVNCSERVER_HAVE_LIBZ)
    list(APPEND COMMON_LIBS ZLIB::ZLIB)
    list(APPEND PC_COMMON_LIBS ${ZLIB_LIBRARIES})
    list(APPEND LIBVNCSERVER_SOURCES
        ${LIBVNCSERVER_DIR}/zlib.c
        ${LIBVNCSERVER_DIR}/zrle.c
        ${LIBVNCSERVER_DIR}/zrleoutstream.c
        ${LIBVNCSERVER_DIR}/zrlepalettehelper.c
        )
endif(ZLIB_FOUND)

if(LZO_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} LZO")
    list(APPEND LIBVNCSERVER_DEFINES LIBVNCSERVER_HAVE_LZO)
    list(APPEND COMMON_LIBS LZO::LZO)
    list(APPEND PC_COMMON_LIBS ${LZO_LIBRARIES})
else()
    list(APPEND LIBVNCSERVER_SOURCES
        ${COMMON_DIR}/minilzo.c
        )
    list(APPEND LIBVNCCLIENT_SOURCES
        ${COMMON_DIR}/minilzo.c
        )
endif()

if(JPEG_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} JPEG")
    list(APPEND COMMON_DEFINES LIBVNCSERVER_HAVE_LIBJPEG)
    list(APPEND COMMON_LIBS JPEG::JPEG)
    list(APPEND PC_COMMON_LIBS ${JPEG_LIBRARIES})
    list(APPEND LIBVNCCLIENT_SOURCES
        ${COMMON_DIR}/turbojpeg.c
        )

  if(PNG_FOUND OR ZLIB_FOUND)
    list(APPEND LIBVNCSERVER_SOURCES ${LIBVNCSERVER_DIR}/tight.c ${COMMON_DIR}/turbojpeg.c)
  endif(PNG_FOUND OR ZLIB_FOUND)
endif(JPEG_FOUND)

if(PNG_FOUND)
    set(LIBVNCSERVER_DEPENDENS "${LIBVNCSERVER_DEPENDENS} PNG")
    list(APPEND LIBVNCSERVER_LIBS PNG::PNG)
    list(APPEND PC_LIBVNCSERVER_LIBS ${PNG_LIBRARIES})
    list(APPEND LIBVNCSERVER_DEFINES LIBVNCSERVER_HAVE_LIBPNG)
endif(PNG_FOUND)

if(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/rfbtightserver.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/handlefiletransferrequest.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/filetransfermsg.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/filelistinfo.c
  )
endif(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)

if(LIBVNCSERVER_WITH_WEBSOCKETS)
  list(APPEND COMMON_DEFINES LIBVNCSERVER_WITH_WEBSOCKETS)
  
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/websockets.c
    ${LIBVNCSERVER_DIR}/ws_decode.c
    ${COMMON_DIR}/base64.c
  )
endif(LIBVNCSERVER_WITH_WEBSOCKETS)

add_library(vncclient ${LIBVNCCLIENT_SOURCES})
add_library(vncserver ${LIBVNCSERVER_SOURCES})
if(WIN32)
    list(APPEND COMMON_LIBS ws2_32)
    list(APPEND PC_COMMON_LIBS ws2_32)
    if(WITH_TIGHTVNC_FILETRANSFER)
        add_definitions(-D_WIN32_WINNT=0x0600)
    endif(WITH_TIGHTVNC_FILETRANSFER)
endif(WIN32)

list(APPEND LIBVNCCLIENT_DEFINES ${COMMON_DEFINES})
target_compile_definitions(vncclient PUBLIC ${LIBVNCCLIENT_DEFINES})
list(APPEND LIBVNCSERVER_DEFINES ${COMMON_DEFINES})
target_compile_definitions(vncserver PUBLIC ${LIBVNCSERVER_DEFINES})
list(APPEND LIBVNCCLIENT_LIBS ${COMMON_LIBS})
target_link_libraries(vncclient PUBLIC ${LIBVNCCLIENT_LIBS})
list(APPEND LIBVNCSERVER_LIBS ${COMMON_LIBS})
target_link_libraries(vncserver PUBLIC ${LIBVNCSERVER_LIBS})

SET_TARGET_PROPERTIES(vncclient vncserver
		PROPERTIES SOVERSION "${VERSION_SO}" VERSION "${LibVNCServer_VERSION}" C_STANDARD 90
)

# EXAMPLES
if(BUILD_EXAMPLES)
    set(LIBVNCSERVER_EXAMPLES
        backchannel
        camera
        cursors
        colourmaptest
        example
        fontsel
        pnmshow
        pnmshow24
        regiontest
        repeater
        rotate
        simple
        simple15
        storepasswd
        vncev
        )
    
    if(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
        list(APPEND LIBVNCSERVER_EXAMPLES blooptest)
    endif(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
    
    if(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
        set(LIBVNCSERVER_EXAMPLES
            ${LIBVNCSERVER_EXAMPLES}
            filetransfer
            )
    endif(WITH_THREADS AND WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
    
    if(APPLE AND NOT IOS AND WITH_THREADS AND CMAKE_USE_PTHREADS_INIT)
        list(APPEND LIBVNCSERVER_EXAMPLES mac)
        find_library(CARBON_LIBRARY Carbon)
        find_library(IOKIT_LIBRARY IOKit)
        find_library(IOSURFACE_LIBRARY IOSurface)
    endif(APPLE AND NOT IOS AND WITH_THREADS AND CMAKE_USE_PTHREADS_INIT)
    
    if(ANDROID)
        list(APPEND LIBVNCSERVER_EXAMPLES androidvncserver)
    endif(ANDROID)
    
    set(LIBVNCCLIENT_EXAMPLES
        backchannel
        ppmtest
        )
    
    if(SDL2_FOUND)
        include_directories(${SDL2_INCLUDE_DIR})
        list(APPEND LIBVNCCLIENT_EXAMPLES SDLvncviewer)
    endif(SDL2_FOUND)
    
    if(GTK2_FOUND)
        include_directories(${GTK2_INCLUDE_DIRS})
        list(APPEND LIBVNCCLIENT_EXAMPLES gtkvncviewer)
    endif(GTK2_FOUND)
    
    if(FFMPEG_FOUND)
        list(APPEND LIBVNCCLIENT_EXAMPLES vnc2mpg)
    endif(FFMPEG_FOUND)
    
    
    foreach(e ${LIBVNCSERVER_EXAMPLES})
        add_executable(examples_${e} ${LIBVNCSRVEXAMPLE_DIR}/${e}.c)
        set_target_properties(examples_${e} PROPERTIES OUTPUT_NAME ${e})
        set_target_properties(examples_${e} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples)
        target_link_libraries(examples_${e} vncserver ${CARBON_LIBRARY} ${IOKIT_LIBRARY} ${IOSURFACE_LIBRARY})
    endforeach(e ${LIBVNCSERVER_EXAMPLES})
    
    foreach(e ${LIBVNCCLIENT_EXAMPLES})
        add_executable(client_examples_${e} ${LIBVNCCLIEXAMPLE_DIR}/${e}.c ${LIBVNCCLIEXAMPLE_DIR}/${${e}_EXTRA_SOURCES} )
        set_target_properties(client_examples_${e} PROPERTIES OUTPUT_NAME ${e})
        set_target_properties(client_examples_${e} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/client_examples)
        target_link_libraries(client_examples_${e} vncclient ${SDL2_LIBRARY} ${GTK2_LIBRARIES} ${FFMPEG_LIBRARIES})
    endforeach(e ${LIBVNCCLIENT_EXAMPLES})
endif(BUILD_EXAMPLES)

#
# them tests
#
if(BUILD_TESTS)
    if(UNIX)
        set(ADDITIONAL_TEST_LIBS m)
    endif(UNIX)
    
    set(SIMPLETESTS
        cargstest
        copyrecttest
        )
    
    if(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
        set(SIMPLETESTS
            ${SIMPLETESTS}
            encodingstest
            )
    endif(WITH_THREADS AND (CMAKE_USE_PTHREADS_INIT OR CMAKE_USE_WIN32_THREADS_INIT))
    
    foreach(t ${SIMPLETESTS})
        add_executable(test_${t} ${TESTS_DIR}/${t}.c)
        set_target_properties(test_${t} PROPERTIES OUTPUT_NAME ${t})
        set_target_properties(test_${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
        target_link_libraries(test_${t} vncserver vncclient ${ADDITIONAL_TEST_LIBS})
    endforeach(t ${SIMPLETESTS})
    
    if(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
        add_executable(test_tjunittest
            ${TESTS_DIR}/tjunittest.c
            ${TESTS_DIR}/tjutil.c
            ${TESTS_DIR}/tjutil.h
            ${COMMON_DIR}/turbojpeg.c
            ${COMMON_DIR}/turbojpeg.h
            )
        set_target_properties(test_tjunittest PROPERTIES OUTPUT_NAME tjunittest)
        set_target_properties(test_tjunittest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
        target_link_libraries(test_tjunittest vncserver vncclient ${ADDITIONAL_TEST_LIBS})
        
        add_executable(test_tjbench
            ${TESTS_DIR}/tjbench.c
            ${TESTS_DIR}/tjutil.c
            ${TESTS_DIR}/tjutil.h
            ${TESTS_DIR}/bmp.c
            ${TESTS_DIR}/bmp.h
            ${COMMON_DIR}/turbojpeg.c
            ${COMMON_DIR}/turbojpeg.h
            )
        set_target_properties(test_tjbench PROPERTIES OUTPUT_NAME tjbench)
        set_target_properties(test_tjbench PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
        target_link_libraries(test_tjbench vncserver vncclient ${ADDITIONAL_TEST_LIBS})
        
    endif(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
    
    if(LIBVNCSERVER_WITH_WEBSOCKETS)
        add_executable(test_wstest
            ${TESTS_DIR}/wstest.c
            ${TESTS_DIR}/wstestdata.inc
            )
        set_target_properties(test_wstest PROPERTIES OUTPUT_NAME wstest)
        set_target_properties(test_wstest PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
        target_link_libraries(test_wstest vncserver vncclient ${ADDITIONAL_TEST_LIBS})
    endif(LIBVNCSERVER_WITH_WEBSOCKETS)
    
    add_test(NAME cargs COMMAND test_cargstest)
    if(UNIX)
        add_test(NAME includetest COMMAND ${TESTS_DIR}/includetest.sh ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR} ${CMAKE_MAKE_PROGRAM})
    endif(UNIX)
    if(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
        add_test(NAME turbojpeg COMMAND test_tjunittest)
    endif(WITH_JPEG AND FOUND_LIBJPEG_TURBO)
    if(LIBVNCSERVER_WITH_WEBSOCKETS)
        add_test(NAME wstest COMMAND test_wstest)
    endif(LIBVNCSERVER_WITH_WEBSOCKETS)
endif(BUILD_TESTS)

#
# this gets the libraries needed by TARGET in "-libx -liby ..." form
#
function(get_link_libraries OUT TARGET)
    set(RESULT "")
    set(LIBRARIES ${${TARGET}})
    foreach(LIB ${LIBRARIES})
	if("${LIB}" MATCHES ".*NOTFOUND.*")
	    continue()
	endif()
	string(REGEX REPLACE "^.*/lib" "" LIB ${LIB}) # remove leading path and "lib" name prefix
	string(REGEX REPLACE "-l" "" LIB ${LIB}) # remove leading -l
	string(REGEX REPLACE "\\.so$" "" LIB ${LIB}) # remove trailing .so
	list(APPEND RESULT "-l${LIB}")
    endforeach()
    list(REMOVE_DUPLICATES RESULT)
    string(CONCAT RESULT ${RESULT}) # back to string
    if(RESULT)
	string(REPLACE "-l" " -l" RESULT ${RESULT}) # re-add separators
    endif(RESULT)
    set(${OUT} ${RESULT} PARENT_SCOPE)
endfunction()

foreach(d ${COMMON_DEFINES})
    set(PC_DEFINES "${PC_DEFINES} -D${d}")
endforeach()
list(APPEND PC_LIBVNCSERVER_LIBS ${PC_COMMON_LIBS})
get_link_libraries(PRIVATE_LIBS PC_LIBVNCSERVER_LIBS)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libvncserver.pc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libvncserver.pc @ONLY)
get_link_libraries(PRIVATE_LIBS PC_COMMON_LIBS)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libvncclient.pc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libvncclient.pc @ONLY)

if(LIBVNCSERVER_INSTALL)
    set(INSTALL_HEADER_FILES
        rfb/keysym.h
        rfb/threading.h
        rfb/rfb.h
        rfb/rfbclient.h
        ${CMAKE_CURRENT_BINARY_DIR}/rfb/rfbconfig.h
        rfb/rfbproto.h
        rfb/rfbregion.h
        )
    
    set_property(TARGET vncclient PROPERTY PUBLIC_HEADER ${INSTALL_HEADER_FILES})
    set_property(TARGET vncserver PROPERTY PUBLIC_HEADER ${INSTALL_HEADER_FILES})
    if(WIN32)
        INSTALL(TARGETS vncclient vncserver
            EXPORT LibVNCServerTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rfb
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
    else()
        INSTALL(TARGETS vncclient vncserver
            EXPORT LibVNCServerTargets
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/rfb
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            )
    endif()
    
    # Install cmake configure files
    install(EXPORT LibVNCServerTargets
        NAMESPACE
            "LibVNCServer::"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LibVNCServer"
        )
    
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
      ${CMAKE_SOURCE_DIR}/cmake/Modules/LibVNCServerConfig.cmake.in
      ${CMAKE_BINARY_DIR}/LibVNCServerConfig.cmake
      INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LibVNCServer"
      )
    # Install cmake version configure file
    write_basic_package_version_file(
        "${CMAKE_BINARY_DIR}/LibVNCServerConfigVersion.cmake"
        COMPATIBILITY AnyNewerVersion)
    install(FILES
        "${CMAKE_BINARY_DIR}/LibVNCServerConfigVersion.cmake"
        "${CMAKE_BINARY_DIR}/LibVNCServerConfig.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/LibVNCServer")
    
  
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/libvncserver.pc
        ${CMAKE_CURRENT_BINARY_DIR}/libvncclient.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
        )
endif()
