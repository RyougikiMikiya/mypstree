cmake_minimum_required(VERSION 3.0.0)
project(pstree VERSION 0.1.0)

include(CTest)
enable_testing()

set(CMAKE_C_STANDARD 11)

add_executable(my_pstree main.c)

# target_compile_options(my_pstree PRIVATE -fsanitize=address -fno-omit-frame-pointer -fsanitize=leak)
# target_link_libraries(my_pstree PRIVATE asan)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
