﻿# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
# Tips: On Unix with make to get more information: make VERBOSE=1
#
cmake_minimum_required (VERSION 3.8)

# Include sub-projects.

# Include the core library of MA
message( "Include util ..." )
add_subdirectory (util)

# Include DB-connector.
message( "Include DB-connector ..." )
add_subdirectory (db_connect)

# kswcpp is always compiled in the Release-mode because without
# optimization it is too slow.
message( "Include libkswcpp ..." )
set( CMAKE_BUILD_TYPE_BACKUP ${CMAKE_BUILD_TYPE} )
if( NOT CMAKE_BUILD_TYPE MATCHES "Release" )
	set( CMAKE_BUILD_TYPE "RelWithDebInfo" )
    message( "Switch for kswcpp to CMAKE_BUILD_TYPE \"RelWithDebInfo\"" )
endif()
add_subdirectory (kswcpp)
set( CMAKE_BUILD_TYPE ${CMAKE_BUILD_TYPE_BACKUP} )

if( ZLIB_FOUND )
    message( "Include libminimizerscpp ..." )
    add_subdirectory (minimizerscpp)
endif()

# Include the core library of MA
message( "Include libMS ..." )
add_subdirectory (ms)
add_subdirectory (ma)
add_subdirectory (msv)
