cmake_minimum_required(VERSION 3.9)
project(azmqn)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.64 COMPONENTS system log date_time thread chrono regex random REQUIRED)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)

if (NOT CMAKE_THREAD_LIBS_INIT)
    set(CMAKE_THREAD_LIBS_INIT "")
endif()

include_directories(${Boost_INCLUDE_DIRS}
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
