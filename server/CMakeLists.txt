add_executable(${PROJECT_NAME}_server main.cpp server.cpp)
target_link_libraries(${PROJECT_NAME}_server pthread)
