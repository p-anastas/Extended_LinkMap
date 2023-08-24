cmake_minimum_required (VERSION 3.10)
### Initialize the project
project (Extended_LinkMap VERSION ${PROJECT_VERSION} DESCRIPTION "Extended_LinkMap project " LANGUAGES CXX CUDA)
message( "------------------------------------------------------------------------------------------------")
message( "Extended_LinkMap - PROJECT_SOURCE_DIR: " ${PROJECT_SOURCE_DIR} )
#message( "Extended_LinkMap - CMAKE_CURRENT_SOURCE_DIR: " ${CMAKE_CURRENT_SOURCE_DIR} )

set ( LINKMAP_DIR ${CMAKE_CURRENT_SOURCE_DIR} )
message( "Extended_LinkMap - LINKMAP_DIR: " ${LINKMAP_DIR} )

set (LINKMAP_DIR_INC ${LINKMAP_DIR}/include)
message( "Extended_LinkMap - Included DIR: " ${LINKMAP_DIR_INC} )
include_directories(${LINKMAP_DIR_INC})

set (LINKMAP_DIR_SRC ${LINKMAP_DIR}/src)
message( "Extended_LinkMap - Source DIR: " ${LINKMAP_DIR_SRC} )

set (BACKEND_LINKMAP_DIR_SRC ${LINKMAP_DIR}/${BACKEND})
message( "Extended_LinkMap - BACKEND DIR: " ${BACKEND_LINKMAP_DIR_SRC} )
message( "Extended_LinkMap - Included DIR: " ${BACKEND_LINKMAP_DIR_SRC} )
include_directories(${BACKEND_LINKMAP_DIR_SRC})

set (LINKMAP_INSTALL ${CMAKE_INSTALL_PROJECT}/Extended_LinkMap)
file(MAKE_DIRECTORY ${LINKMAP_INSTALL})

set (LINKMAP_INSTALL_LIB ${LINKMAP_INSTALL}/lib)
file(MAKE_DIRECTORY ${LINKMAP_INSTALL_LIB})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${LINKMAP_INSTALL_LIB})

set (LINKMAP_INSTALL_INC ${LINKMAP_INSTALL}/include)
file(MAKE_DIRECTORY ${LINKMAP_INSTALL_INC})

## Enable using lazy events; e.g. event backends are created on first record on the corresponding device.
#FIXME: Current CoCoPeLia version only work with lazy events.
add_definitions(-DENABLE_LAZY_EVENTS)

if(${BUILD_BLAS_WRAPPERS_FLAG} EQUAL 1)
    add_library(linkmap_helpers SHARED
    ${LINKMAP_DIR_SRC}/general_utils.cpp
    ${BACKEND_LINKMAP_DIR_SRC}/backend_util_wrappers.cu
    ${BACKEND_LINKMAP_DIR_SRC}/backend_data_wrappers.cu
    ${BACKEND_LINKMAP_DIR_SRC}/backend_class_wrappers.cu
    ${BACKEND_LINKMAP_DIR_SRC}/backend_lib_wrappers.cu
    ${BACKEND_LINKMAP_DIR_SRC}/backend_routine_wrappers.cu
    )
else()
    add_library(linkmap_helpers SHARED
    ${LINKMAP_DIR_SRC}/general_utils.cpp
    ${BACKEND_LINKMAP_DIR_SRC}/backend_util_wrappers.cu
    ${BACKEND_LINKMAP_DIR_SRC}/backend_data_wrappers.cu
    ${BACKEND_LINKMAP_DIR_SRC}/backend_class_wrappers.cu
    )
endif()

ADD_SUBDIRECTORY (Software-assisted_Memory_Buffer)
include_directories(${CACHING_INSTALL_INC})
message( "Extended_LinkMap - Included DIR: " ${CACHING_INSTALL_INC} )
set (CACHING_INSTALL_INC ${CACHING_INSTALL_INC} PARENT_SCOPE)

add_library(linkmap SHARED
    ${LINKMAP_DIR_SRC}/chain_transfer.cpp
    ${LINKMAP_DIR_SRC}/LinkRoute.cpp
    ${LINKMAP_DIR_SRC}/Tile1D.cpp
    ${LINKMAP_DIR_SRC}/Tile2D.cpp
    ${LINKMAP_DIR_SRC}/DataTile.cpp
)

target_link_libraries(linkmap ${INTERNAL_LINK_LIB} ${OPENBLAS_LINK} linkmap_helpers caching)
configure_file(${LINKMAP_DIR_INC}/linkmap.hpp ${LINKMAP_INSTALL_INC}/linkmap.hpp COPYONLY)
configure_file(${LINKMAP_DIR_INC}/DataTile.hpp ${LINKMAP_INSTALL_INC}/DataTile.hpp COPYONLY)

## FIXME: This is a temporary cheat for deployment, must be removed at some point (?). It has to do with TransposeTranslate.
configure_file(${BACKEND_LINKMAP_DIR_SRC}/backend_wrappers.hpp ${LINKMAP_INSTALL_INC}/backend_wrappers.hpp COPYONLY)

set (LINKMAP_INSTALL_INC ${LINKMAP_INSTALL_INC} PARENT_SCOPE)
message( "->\nExtended_LinkMap - Shared Library: " ${CMAKE_LIBRARY_OUTPUT_DIRECTORY} )
message( "Extended_LinkMap - Shared Library headers: " ${LINKMAP_INSTALL_INC} )
message( "------------------------------------------------------------------------------------------------")

#set_target_properties(linkmap PROPERTIES VERSION ${PROJECT_VERSION})
#set_target_properties(linkmap PROPERTIES SOVERSION 0)
#set_target_properties(linkmap PROPERTIES PUBLIC_HEADER ${PROJECT_BACKEND_LIB_DIR_INC}/linkmap.hpp)
#target_include_directories(mylib PRIVATE .)
#target_include_directories(mylib PRIVATE include)
#target_include_directories(mylib PRIVATE src)

#include(GNUInstallDirs)
#install(TARGETS linkmap
#    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
#    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
#configure_file(linkmap.pc.in linkmap.pc @ONLY)
#install(FILES ${CMAKE_BINARY_DIR}/linkmap.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)
