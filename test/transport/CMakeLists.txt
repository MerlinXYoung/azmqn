project(test_transport)

add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})

add_catch_test(${PROJECT_NAME})
