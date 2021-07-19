# Find liblzo2

# IMPORTED Targets
# ^^^^^^^^^^^^^^^^

# This module defines :prop_tgt:`IMPORTED` target ``LZO::LZO``, if
# LZO has been found.

# Result Variables
# ^^^^^^^^^^^^^^^^
# This module defines the following variables:

# LZO_FOUND - system has the LZO library
# LZO_INCLUDE_DIRS - the LZO include directory
# LZO_LIBRARIES - The libraries needed to use LZO

if (LZO_INCLUDE_DIR AND LZO_LIBRARIES)
	# in cache already
	SET(LZO_FOUND TRUE)
else (LZO_INCLUDE_DIR AND LZO_LIBRARIES)
	FIND_PATH(LZO_INCLUDE_DIR NAMES lzo/lzo1x.h)

	FIND_LIBRARY(LZO_LIBRARIES NAMES lzo2)

	if (LZO_INCLUDE_DIR AND LZO_LIBRARIES)
		 set(LZO_FOUND TRUE)
	endif (LZO_INCLUDE_DIR AND LZO_LIBRARIES)

	if (LZO_FOUND)
		 if (NOT LZO_FIND_QUIETLY)
				message(STATUS "Found LZO: ${LZO_LIBRARIES}")
		 endif (NOT LZO_FIND_QUIETLY)
	else (LZO_FOUND)
		 if (LZO_FIND_REQUIRED)
				message(FATAL_ERROR "Could NOT find LZO")
         else()
				message(STATUS "Could NOT find LZO")
		 endif (LZO_FIND_REQUIRED)
	endif (LZO_FOUND)

#	MARK_AS_ADVANCED(LZO_INCLUDE_DIR LZO_LIBRARIES)
endif (LZO_INCLUDE_DIR AND LZO_LIBRARIES)

if(LZO_FOUND)
    set(LZO_INCLUDE_DIRS ${LZO_INCLUDE_DIR})
    
    if(NOT TARGET LZO::LZO)
        add_library(LZO::LZO UNKNOWN IMPORTED)
        set_target_properties(LZO::LZO PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LZO_INCLUDE_DIRS}")
        
        set_property(TARGET LZO::LZO APPEND PROPERTY
            IMPORTED_LOCATION "${LZO_LIBRARIES}")
        
    endif()
endif()
