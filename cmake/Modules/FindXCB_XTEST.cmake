# This module defines
# XCB_XTEST_FOUND, if false, do not try to link to xcb-xtest
# XCB_XTEST_INCLUDE_DIR, where to find xtest.h
# XCB_XTEST_LIBRARY, the name of the library to link against

if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
	set(LIB_PATH_SUFFIXES lib64 lib/x64 lib)
else() 
	set(LIB_PATH_SUFFIXES lib/x86 lib)
endif() 

# find xcb-xtest
FIND_PATH(XCB_XTEST_INCLUDE_DIR  xcb/xtest.h)
FIND_LIBRARY(
    XCB_XTEST_LIBRARY
    xcb-xtest
    PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
)

# set XCB_XTEST_FOUND
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    XCB_XTEST 
    REQUIRED_VARS 
    XCB_XTEST_INCLUDE_DIR
    XCB_XTEST_LIBRARY
)