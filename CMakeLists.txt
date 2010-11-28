project(libvncserver)

macro_optional_find_package(ZLIB)
macro_log_feature(ZLIB_FOUND "ZLib" "The Zlib compression library" "http://www.zlib.net" FALSE "" "Used by the vncserver library.")

macro_optional_find_package(JPEG)
macro_log_feature(JPEG_FOUND "LibJPEG" "The JPEG library" "" FALSE "" "Used by the vncserver library")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckCXXSourceCompiles)
include(TestBigEndian)

macro_bool_to_01(ZLIB_FOUND LIBVNCSERVER_HAVE_LIBZ)
macro_bool_to_01(JPEG_FOUND LIBVNCSERVER_HAVE_LIBJPEG)

check_include_files(fcntl.h       LIBVNCSERVER_HAVE_FCNTL_H)
check_include_files(unistd.h      LIBVNCSERVER_HAVE_UNISTD_H)
check_include_files(sys/socket.h  LIBVNCSERVER_HAVE_SYS_SOCKET_H)
check_include_files(sys/types.h   LIBVNCSERVER_HAVE_SYS_TYPES_H)
check_include_files(sys/stat.h    LIBVNCSERVER_HAVE_SYS_STAT_H)
check_include_files(sys/time.h    LIBVNCSERVER_HAVE_SYS_TIME_H)
check_include_files(netinet/in.h  LIBVNCSERVER_HAVE_NETINET_IN_H)
check_include_files(stdint.h      LIBVNCSERVER_HAVE_STDINT_H)
check_include_files(ifaddrs.h     LIBVNCSERVER_HAVE_IFADDRS_H)

check_function_exists(gettimeofday LIBVNCSERVER_HAVE_GETTIMEOFDAY)

test_big_endian(LIBVNCSERVER_WORDS_BIGENDIAN)

set(LIBVNCSERVER_PACKAGE_STRING  "Krfb ${KDE_VERSION} - LibVNCServer 0.9.7")

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/libvncserver-config.h.cmake
                ${CMAKE_CURRENT_BINARY_DIR}/libvncserver-config.h
)


set(LIBVNCSERVER_SRCS
    main.c
    rfbserver.c
    rfbregion.c
    auth.c
    sockets.c
    stats.c
    corre.c
    hextile.c
    rre.c
    translate.c
    cutpaste.c
    httpd.c
    cursor.c
    font.c
    draw.c
    selbox.c
    d3des.c
    vncauth.c
    cargs.c
    minilzo.c
    ultra.c
    scale.c
)

if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
    set(LIBVNCSERVER_SRCS ${LIBVNCSERVER_SRCS} zlib.c zrle.c zrleoutstream.c zrlepalettehelper.c zywrletemplate.c)

    if (JPEG_FOUND)
        include_directories(${JPEG_INCLUDE_DIR})
        set(LIBVNCSERVER_SRCS ${LIBVNCSERVER_SRCS} tight.c)
    endif()
endif()


add_library(vncserver STATIC ${LIBVNCSERVER_SRCS})


if (ZLIB_FOUND)
    target_link_libraries(vncserver ${ZLIB_LIBRARIES})

    if (JPEG_FOUND)
        target_link_libraries(vncserver ${JPEG_LIBRARIES})
    endif()
endif()
