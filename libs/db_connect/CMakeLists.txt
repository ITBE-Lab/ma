cmake_minimum_required (VERSION 3.8)

project("db_connect" VERSION 0.1 DESCRIPTION "Database connection library")

#INCLUDE(cmake/FindMySQL.cmake)

if(WITH_DB)

    # Check for PostgreSQL Support
    # (With Windows, PostgreSQL DLL location must be added to search path. (bin and lib folders of installation)) 
    find_package(PostgreSQL)

    if( PostgreSQL_FOUND )
        file(GLOB DB_CONNECT_SRC
            "sql_global_context.cpp"
            "connectors/postgre_global_context.cpp"
        ) # file
    else()
        # In the case that there is not PostgreSQL, we go for MySQL
        if( MySQL_FOUND )
            file(GLOB DB_CONNECT_SRC
                "sql_global_context.cpp"
            ) # file
        endif()
    endif()

    if(NOT (PostgreSQL_FOUND OR MySQL_FOUND) )
        message(FATAL_ERROR "Could detect neither MySQL nor PostgreSQL ")
    endif()


    # Define the target dbSQL
    add_library(dbSQL SHARED ${DB_CONNECT_SRC})
    target_compile_definitions( dbSQL PUBLIC DLL_PORT_util=dllimport )

    # enable code ifdefs
    target_compile_definitions( dbSQL PUBLIC WITH_DB )

    # link to the fort object file
    target_link_libraries ( dbSQL PUBLIC Threads::Threads fort util contrib_CPP_header )
    # location of all includes 
    target_include_directories( dbSQL PUBLIC connectors . )


    # if( MySQL_FOUND )
    #     message( "Found MySQL includes and libraries")
    #     target_link_libraries ( dbSQL PUBLIC ${MYSQL_LIBRARIES} )
    #     target_include_directories( dbSQL PUBLIC ${MYSQL_INCLUDE_DIRS} )
    # endif()

    if( PostgreSQL_FOUND )
        message( "Found PostgreSQL - library: " ${PostgreSQL_LIBRARIES} )
        target_link_libraries ( dbSQL PUBLIC ${PostgreSQL_LIBRARIES} )
        target_include_directories( dbSQL PUBLIC ${PostgreSQL_INCLUDE_DIRS} )
    endif()

    # Integration of test-code, if requested
    if(BUILD_TESTING)
        # add tests for all cpp files in the tests folder
        # Get all .cpp files in the test folder
        file(GLOB MA_CPP_TEST_SRC
            "tests/*.cpp"
        ) # file
        
        foreach(file ${MA_CPP_TEST_SRC})
            get_filename_component(test_name ${file} NAME)
            message( "-- MA Database Connector: generating test " ${test_name} )
            add_executable( ${test_name} ${file} )
            target_link_libraries( ${test_name} PUBLIC dbSQL)
            # and for each of your dependent executable/library targets:
            if( ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") AND (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0) )
                # std::filesystem is still experimental and for linkage we need separated library
                message( "-- libMA: Filesystem support for GCC below version 8" )
                target_link_libraries( ${test_name} PUBLIC stdc++fs )
            endif()
            add_test( ${test_name} ${test_name} )
        endforeach()
    endif()
endif()


