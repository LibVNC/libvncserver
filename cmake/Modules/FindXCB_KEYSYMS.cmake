# This module defines
# XCB_KEYSYMS_FOUND, if false, do not try to link to xcb-keysyms
# XCB_KEYSYMS_INCLUDE_DIR, where to find xcb_keysyms.h
# XCB_KEYSYMS_LIBRARY, the name of the library to link against

if(CMAKE_SIZEOF_VOID_P EQUAL 8) 
	set(LIB_PATH_SUFFIXES lib64 lib/x64 lib)
else() 
	set(LIB_PATH_SUFFIXES lib/x86 lib)
endif() 

# find xcb-keysyms
FIND_PATH(XCB_KEYSYMS_INCLUDE_DIR  xcb/xcb_keysyms.h)
FIND_LIBRARY(
    XCB_KEYSYMS_LIBRARY
    xcb-keysyms
    PATH_SUFFIXES ${LIB_PATH_SUFFIXES}
)

# set XCB_KEYSYMS_FOUND
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    XCB_KEYSYMS
    REQUIRED_VARS 
    XCB_KEYSYMS_INCLUDE_DIR
    XCB_KEYSYMS_LIBRARY
)