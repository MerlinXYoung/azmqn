cmake_minimum_required(VERSION 3.9)
project(azmqn)

# use ccache if present
find_program(CCACHE_FOUND ccache)
if (CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif()

set(CMAKE_CXX_STANDARD 17)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

#find_package(Boost 1.65.1 COMPONENTS system thread REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

if (NOT CMAKE_THREAD_LIBS_INIT)
    set(CMAKE_THREAD_LIBS_INIT "")
endif()

include_directories(${CONAN_INCLUDE_DIRS}
                    ${PROJECT_SOURCE_DIR})

enable_testing()

macro(add_catch_test name)
    if (TEST_REPORT_FORMAT)
        add_test(NAME ${name} COMMAND ${name} -r ${TEST_REPORT_FORMAT} -o "${name}.test_out.xml")
    else()
        add_test(NAME ${name} COMMAND ${name})
    endif()
endmacro()

add_subdirectory(test)
#add_subdirectory(doc)

#install(DIRECTORY ${PROJECT_SOURCE_DIR}/azmqn
#        DESTINATION include)
