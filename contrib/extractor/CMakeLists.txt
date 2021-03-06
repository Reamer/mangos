# Copyright (C) 2005-2012 MaNGOS project <http://getmangos.com/>
#
# This file is free software; as a special exception the author gives
# unlimited permission to copy and/or distribute it, with or without
# modifications, as long as this notice is preserved.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY, to the extent permitted by law; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

cmake_minimum_required (VERSION 2.6)
project (MANGOS_MAP_EXTRACTOR)

add_subdirectory ("${MANGOS_MAP_EXTRACTOR_SOURCE_DIR}/../../dep/StormLib" "${MANGOS_MAP_EXTRACTOR_SOURCE_DIR}/build/StromLib")
add_subdirectory (loadlib)

include_directories (${MANGOS_MAP_EXTRACTOR_SOURCE_DIR}/../../dep/StormLib/src)
include_directories (${MANGOS_MAP_EXTRACTOR_SOURCE_DIR}/loadlib)

link_directories (${MANGOS_MAP_EXTRACTOR_SOURCE_DIR}/../../dep/StormLib)
link_directories (${MANGOS_MAP_EXTRACTOR_SOURCE_DIR}/loadlib)

add_executable (extractor dbcfile.cpp extractor.cpp util.cpp)

if(WIN32)
    target_link_libraries (extractor StormLib)
else()
    target_link_libraries (extractor storm)
endif()

target_link_libraries (extractor loadlib)