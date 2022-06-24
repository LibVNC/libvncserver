# Find liblzo2
# LZO_FOUND - system has the LZO library
# LZO_INCLUDE_DIR - the LZO include directory
# LZO_LIBRARIES - The libraries needed to use LZO

FIND_PATH(LZO_INCLUDE_DIR NAMES lzo/lzo1x.h)

FIND_LIBRARY(LZO_LIBRARIES NAMES lzo2)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LZO
  REQUIRED_VARS LZO_LIBRARIES LZO_INCLUDE_DIR
)

if (LZO_FOUND AND NOT TARGET LZO::LZO)
    add_library(LZO::LZO UNKNOWN IMPORTED)
    set_target_properties(LZO::LZO PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${LZO_INCLUDE_DIR}")

    set_property(TARGET LZO::LZO APPEND PROPERTY
        IMPORTED_LOCATION "${LZO_LIBRARIES}")
endif()
