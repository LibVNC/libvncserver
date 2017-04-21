cmake_minimum_required(VERSION 3.4)
cmake_policy(SET CMP0037 NEW)

project(LibVNCServer)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckTypeSize)
include(TestBigEndian)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)

enable_testing()

set(PACKAGE_NAME           "LibVNCServer")
set(FULL_PACKAGE_NAME      "LibVNCServer")
set(VERSION_MAJOR          "0")
set(VERSION_MINOR          "9")
set(VERSION_PATCHLEVEL     "12")
set(VERSION_SO             "1")
set(PACKAGE_VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCHLEVEL}")
set(PROJECT_BUGREPORT_PATH "https://github.com/LibVNC/libvncserver/issues")
set(LIBVNCSERVER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libvncserver)
set(COMMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common)
set(LIBVNCCLIENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libvncclient)
set(LIBVNCSRVEXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)
set(LIBVNCCLIEXAMPLE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/client_examples)
set(TESTS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/test)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")


include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/libvncserver ${CMAKE_CURRENT_SOURCE_DIR}/common)

# all the build configuration switches
option(BUILD_SHARED_LIBS "Build shared libraries" ${UNIX})
option(WITH_ZLIB "Search for the zlib compression library to support additional encodings" ON)
option(WITH_JPEG "Search for the libjpeg compression library to support additional encodings" ON)
option(WITH_PNG "Search for the PNG compression library to support additional encodings" ON)
option(WITH_SDL "Search for the Simple Direct Media Layer library to build an example SDL vnc client" ON)
option(WITH_THREADS "Search for a threading library to build with multithreading support" ON)
option(WITH_GNUTLS "Search for the GnuTLS secure communications library to support encryption" ON)
option(WITH_OPENSSL "Search for the OpenSSL cryptography library to support encryption" ON)
option(WITH_SYSTEMD "Search for libsystemd to build with systemd socket activation support" ON)
option(WITH_GCRYPT "Search for libgcrypt to support additional authentication methods in LibVNCClient" ON)
option(WITH_FFMPEG "Search for FFMPEG to build an example VNC to MPEG encoder" ON)
option(WITH_TIGHTVNC_FILETRANSFER "Enable filetransfer if there is pthreads support" ON)
option(WITH_24BPP "Allow 24 bpp" ON)
option(WITH_IPv6 "Enable IPv6 Support" ON)
option(WITH_WEBSOCKETS "Build with websockets support" ON)



if(WITH_ZLIB)
  find_package(ZLIB)
endif(WITH_ZLIB)


if(WITH_JPEG)
  find_package(JPEG)
  # Check whether the version of libjpeg we found was libjpeg-turbo and print a
  # warning if not.
  set(CMAKE_REQUIRED_LIBRARIES ${JPEG_LIBRARIES})
  set(CMAKE_REQUIRED_FLAGS -I${JPEG_INCLUDE_DIR})

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
  set(CMAKE_REQUIRED_FLAGS)
  set(CMAKE_REQUIRED_DEFINITIONS)

  if(NOT FOUND_LIBJPEG_TURBO)
    message(WARNING "*** The libjpeg library you are building against is not libjpeg-turbo.  Performance will be reduced.  You can obtain libjpeg-turbo from:  https://sourceforge.net/projects/libjpeg-turbo/files/ ***")
  endif()
endif(WITH_JPEG)


if(WITH_PNG)
  find_package(PNG)
endif(WITH_PNG)


if(WITH_SDL)
  find_package(SDL)
endif(WITH_SDL)


if(WITH_THREADS)
  find_package(Threads)
endif(WITH_THREADS)


if(WITH_GNUTLS)
  find_package(GnuTLS)
endif(WITH_GNUTLS)


if(WITH_OPENSSL)
  find_package(OpenSSL)
endif(WITH_OPENSSL)


if(WITH_SYSTEMD AND NOT ANDROID)
  find_package(PkgConfig)
  pkg_check_modules(SYSTEMD "libsystemd")
endif(WITH_SYSTEMD AND NOT ANDROID)


if(WITH_GCRYPT)
  find_library(LIBGCRYPT_LIBRARIES gcrypt)
endif(WITH_GCRYPT)

if(WITH_FFMPEG)
  find_package(FFMPEG)
endif(WITH_FFMPEG)


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
check_include_file("sys/uio.h"     LIBVNCSERVER_HAVE_SYS_UIO_H)


# headers needed for check_type_size()
check_include_file("vfork.h"       LIBVNCSERVER_HAVE_VFORK_H)
check_include_file("ws2tcpip.h"    LIBVNCSERVER_HAVE_WS2TCPIP_H)
check_include_file("arpa/inet.h"   HAVE_ARPA_INET_H)
check_include_file("stdint.h"      HAVE_STDINT_H)
check_include_file("stddef.h"      HAVE_STDDEF_H)
check_include_file("sys/types.h"   HAVE_SYS_TYPES_H)

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

# On systems such as GNU/Linux with glibc, __b64_ntop is defined in a
# separate library, libresolv. On some others, such as FreeBSD, it is
# part of libc itself. We first check if __b64_ntop is found without
# additional libraries, and then try looking for it with libresolv if
# the first test fails.
check_function_exists(__b64_ntop HAVE_B64_IN_LIBC)
if(NOT HAVE_B64_IN_LIBC)
    set(CMAKE_REQUIRED_LIBRARIES resolv)
    check_function_exists(__b64_ntop HAVE_B64_IN_LIBRESOLV)
    set(CMAKE_REQUIRED_LIBRARIES)

    if(HAVE_B64_IN_LIBRESOLV)
        set(RESOLV_LIB "resolv")
    endif(HAVE_B64_IN_LIBRESOLV)

    # the function check somehow fails for apple but the function is there
    if(APPLE)
        set(RESOLV_LIB "resolv")
    endif(APPLE)

endif(NOT HAVE_B64_IN_LIBC)

if(Threads_FOUND)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(Threads_FOUND)
if(ZLIB_FOUND)
  set(LIBVNCSERVER_HAVE_LIBZ 1)
else()
  unset(ZLIB_LIBRARIES) # would otherwise contain -NOTFOUND, confusing target_link_libraries()
endif(ZLIB_FOUND)
if(JPEG_FOUND)
  set(LIBVNCSERVER_HAVE_LIBJPEG 1)
endif(JPEG_FOUND)
if(PNG_FOUND)
  set(LIBVNCSERVER_HAVE_LIBPNG 1)
endif(PNG_FOUND)
if(NOT OPENSSL_FOUND)
    unset(OPENSSL_LIBRARIES) # would otherwise contain -NOTFOUND, confusing target_link_libraries()
endif()
if(SYSTEMD_FOUND)
  add_definitions(-DLIBVNCSERVER_WITH_SYSTEMD)
  include_directories(${SYSTEMD_INCLUDE_DIRS})
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${SYSTEMD_LIBRARIES})
endif(SYSTEMD_FOUND)


if(WITH_WEBSOCKETS AND LIBVNCSERVER_HAVE_SYS_UIO_H)
  set(LIBVNCSERVER_WITH_WEBSOCKETS 1)
  if(GNUTLS_FOUND)
    set(LIBVNCSERVER_WITH_CLIENT_TLS 1)
    message(STATUS "Building websockets with GnuTLS")
    set(WEBSOCKET_LIBRARIES ${RESOLV_LIB} ${GNUTLS_LIBRARIES})
    set(WSSRCS ${LIBVNCSERVER_DIR}/rfbssl_gnutls ${LIBVNCSERVER_DIR}/rfbcrypto_gnutls)
    include_directories(${GNUTLS_INCLUDE_DIR})
  elseif(OPENSSL_FOUND)
    message(STATUS "Building websockets with OpenSSL")
    set(WEBSOCKET_LIBRARIES ${RESOLV_LIB} ${OPENSSL_LIBRARIES})
    set(WSSRCS ${LIBVNCSERVER_DIR}/rfbssl_openssl ${LIBVNCSERVER_DIR}/rfbcrypto_openssl)
  else()
    message(STATUS "Building websockets without SSL")
    set(WEBSOCKET_LIBRARIES ${RESOLV_LIB})
    set(WSSRCS ${LIBVNCSERVER_DIR}/rfbssl_none.c ${LIBVNCSERVER_DIR}/rfbcrypto_included.c ${COMMON_DIR}/md5.c ${COMMON_DIR}/sha1.c)
  endif()
endif(WITH_WEBSOCKETS AND LIBVNCSERVER_HAVE_SYS_UIO_H)

if(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)
  message(STATUS "Found libgcrypt: ${LIBGCRYPT_LIBRARIES}")
  set(LIBVNCSERVER_WITH_CLIENT_GCRYPT 1)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ${LIBGCRYPT_LIBRARIES})
endif(WITH_GCRYPT AND LIBGCRYPT_LIBRARIES)

if(OPENSSL_FOUND)
  set(LIBVNCSERVER_HAVE_LIBSSL 1)
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


if(CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_HAVE_LIBPTHREAD 1)
endif(CMAKE_USE_PTHREADS_INIT)
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

# TODO:
# LIBVNCSERVER_ENOENT_WORKAROUND
# inline

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/rfb/rfbconfig.h.cmakein ${CMAKE_BINARY_DIR}/rfb/rfbconfig.h)

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
    ${COMMON_DIR}/d3des.c
    ${COMMON_DIR}/vncauth.c
    ${LIBVNCSERVER_DIR}/cargs.c
    ${COMMON_DIR}/minilzo.c
    ${LIBVNCSERVER_DIR}/ultra.c
    ${LIBVNCSERVER_DIR}/scale.c
)

set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_DIR}/cursor.c
    ${LIBVNCCLIENT_DIR}/listen.c
    ${LIBVNCCLIENT_DIR}/rfbproto.c
    ${LIBVNCCLIENT_DIR}/sockets.c
    ${LIBVNCCLIENT_DIR}/vncviewer.c
    ${COMMON_DIR}/minilzo.c
)

if(GNUTLS_FOUND)
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/tls_gnutls.c
  )
elseif(OPENSSL_FOUND)
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/tls_openssl.c
  )
  include_directories(${OPENSSL_INCLUDE_DIR})
else()
  set(LIBVNCCLIENT_SOURCES
    ${LIBVNCCLIENT_SOURCES}
    ${LIBVNCCLIENT_DIR}/tls_none.c
  )
endif()

if(ZLIB_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBZ)
  include_directories(${ZLIB_INCLUDE_DIR})
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/zlib.c
    ${LIBVNCSERVER_DIR}/zrle.c
    ${LIBVNCSERVER_DIR}/zrleoutstream.c
    ${LIBVNCSERVER_DIR}/zrlepalettehelper.c
  )
endif(ZLIB_FOUND)

if(JPEG_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBJPEG)
  include_directories(${JPEG_INCLUDE_DIR})
  set(TIGHT_C ${LIBVNCSERVER_DIR}/tight.c ${COMMON_DIR}/turbojpeg.c)
endif(JPEG_FOUND)

if(PNG_FOUND)
  add_definitions(-DLIBVNCSERVER_HAVE_LIBPNG)
  include_directories(${PNG_INCLUDE_DIR})
endif(PNG_FOUND)

set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${TIGHT_C}
)

if(WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/rfbtightserver.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/handlefiletransferrequest.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/filetransfermsg.c
    ${LIBVNCSERVER_DIR}/tightvnc-filetransfer/filelistinfo.c
  )
endif(WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)

if(LIBVNCSERVER_WITH_WEBSOCKETS)
  add_definitions(-DLIBVNCSERVER_WITH_WEBSOCKETS)
  set(LIBVNCSERVER_SOURCES
    ${LIBVNCSERVER_SOURCES}
    ${LIBVNCSERVER_DIR}/websockets.c
    ${WSSRCS}
  )
endif(LIBVNCSERVER_WITH_WEBSOCKETS)


add_library(vncclient ${LIBVNCCLIENT_SOURCES})
add_library(vncserver ${LIBVNCSERVER_SOURCES})
if(WIN32)
  set(ADDITIONAL_LIBS ${ADDITIONAL_LIBS} ws2_32)
endif(WIN32)

target_link_libraries(vncclient
                      ${ADDITIONAL_LIBS}
                      ${ZLIB_LIBRARIES}
                      ${JPEG_LIBRARIES}
                      ${GNUTLS_LIBRARIES}
                      ${OPENSSL_LIBRARIES}
)
target_link_libraries(vncserver
                      ${ADDITIONAL_LIBS}
                      ${ZLIB_LIBRARIES}
                      ${JPEG_LIBRARIES}
		      ${PNG_LIBRARIES}
		      ${WEBSOCKET_LIBRARIES}
)

SET_TARGET_PROPERTIES(vncclient vncserver
		PROPERTIES SOVERSION "${VERSION_SO}" VERSION "${PACKAGE_VERSION}"
)

# EXAMPLES
set(LIBVNCSERVER_EXAMPLES
    backchannel
    camera
    colourmaptest
    example
    fontsel
    pnmshow
    pnmshow24
    regiontest
    rotate
    simple
    simple15
    storepasswd
    vncev
  )

if(CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    blooptest
  )
endif(CMAKE_USE_PTHREADS_INIT)

if(WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    filetransfer
  )
endif(WITH_TIGHTVNC_FILETRANSFER AND CMAKE_USE_PTHREADS_INIT)

if(MACOS)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    mac
  )
endif(MACOS)

if(ANDROID)
  set(LIBVNCSERVER_EXAMPLES
    ${LIBVNCSERVER_EXAMPLES}
    androidvncserver
  )
endif(ANDROID)

set(LIBVNCCLIENT_EXAMPLES
    backchannel
    ppmtest
)

if(SDL_FOUND)
  include_directories(${SDL_INCLUDE_DIR})
  set(LIBVNCCLIENT_EXAMPLES
    ${LIBVNCCLIENT_EXAMPLES}
    SDLvncviewer
  )
  set(SDLvncviewer_EXTRA_SOURCES scrap.c)
endif(SDL_FOUND)

if(FFMPEG_FOUND)
  set(LIBVNCCLIENT_EXAMPLES
    ${LIBVNCCLIENT_EXAMPLES}
    vnc2mpg
  )
endif(FFMPEG_FOUND)


file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples)
foreach(e ${LIBVNCSERVER_EXAMPLES})
  add_executable(examples_${e} ${LIBVNCSRVEXAMPLE_DIR}/${e}.c)
  set_target_properties(examples_${e} PROPERTIES OUTPUT_NAME ${e})
  set_target_properties(examples_${e} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples)
  target_link_libraries(examples_${e} vncserver ${CMAKE_THREAD_LIBS_INIT})
endforeach(e ${LIBVNCSERVER_EXAMPLES})

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/client_examples)
foreach(e ${LIBVNCCLIENT_EXAMPLES})
  add_executable(client_examples_${e} ${LIBVNCCLIEXAMPLE_DIR}/${e}.c ${LIBVNCCLIEXAMPLE_DIR}/${${e}_EXTRA_SOURCES} )
  set_target_properties(client_examples_${e} PROPERTIES OUTPUT_NAME ${e})
  set_target_properties(client_examples_${e} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/client_examples)
  target_link_libraries(client_examples_${e} vncclient ${CMAKE_THREAD_LIBS_INIT} ${SDL_LIBRARY} ${FFMPEG_LIBRARIES})
endforeach(e ${LIBVNCCLIENT_EXAMPLES})


#
# them tests
#

if(UNIX)
    set(ADDITIONAL_TEST_LIBS m)
endif(UNIX)

set(SIMPLETESTS
   cargstest
   copyrecttest
)

if(CMAKE_USE_PTHREADS_INIT)
  set(SIMPLETESTS
      ${SIMPLETESTS}
      encodingstest
     )
endif(CMAKE_USE_PTHREADS_INIT)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/test)
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

add_test(NAME cargs COMMAND test_cargstest)
if(FOUND_LIBJPEG_TURBO)
    add_test(NAME turbojpeg COMMAND test_tjunittest)
endif(FOUND_LIBJPEG_TURBO)



#
# this gets the libraries needed by TARGET in "-libx -liby ..." form
#
function(get_link_libraries OUT TARGET)
    set(RESULT "")
    get_target_property(LIBRARIES ${TARGET} INTERFACE_LINK_LIBRARIES)
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
    string(REPLACE "-l" " -l" RESULT ${RESULT}) # re-add separators
    set(${OUT} ${RESULT} PARENT_SCOPE)
endfunction()

get_link_libraries(PRIVATE_LIBS vncserver)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libvncserver.pc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libvncserver.pc @ONLY)
get_link_libraries(PRIVATE_LIBS vncclient)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/libvncclient.pc.cmakein ${CMAKE_CURRENT_BINARY_DIR}/libvncclient.pc @ONLY)


install_targets(/lib vncserver)
install_targets(/lib vncclient)
install_files(/include/rfb FILES
    rfb/keysym.h
    rfb/rfb.h
    rfb/rfbclient.h
    rfb/rfbconfig.h
    rfb/rfbproto.h
    rfb/rfbregion.h
)

install_files(/lib/pkgconfig FILES
    libvncserver.pc
    libvncclient.pc
)
