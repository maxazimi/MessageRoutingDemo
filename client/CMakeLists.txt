add_executable(${PROJECT_NAME}_client main.cpp client.cpp)
target_link_libraries(${PROJECT_NAME}_client pthread)
