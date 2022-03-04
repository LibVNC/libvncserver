# This module defines
# XCB_FOUND, if false, do not try to link to xcb
# XCB_INCLUDE_DIR, where to find xcb.h
# XCB_LIBRARY, the name of the library to link against

if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
	set(LIB_PATH_SUFFIXES lib64 lib/x64 lib)
else() 
	set(LIB_PATH_SUFFIXES lib/x86 lib)
endif() 

# find xcb
FIND_PATH(XCB_INCLUDE_DIR xcb/xcb.h)
FIND_LIBRARY(
    XCB_LIBRARY
    xcb 
    PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
)

# set XCB_FOUND
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    XCB 
    REQUIRED_VARS 
    XCB_INCLUDE_DIR
    XCB_LIBRARY
)