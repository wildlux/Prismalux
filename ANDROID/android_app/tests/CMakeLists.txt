cmake_minimum_required(VERSION 3.16)
project(PrismaluxMobileTests)

set(CMAKE_CXX_STANDARD 17)

# Usa Qt6 di sistema se Qt6_DIR non è specificato esplicitamente
if(NOT DEFINED Qt6_DIR)
    set(Qt6_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt6")
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Test)

qt6_add_executable(test_mobile_logic test_mobile_logic.cpp)
set_target_properties(test_mobile_logic PROPERTIES AUTOMOC ON)
target_link_libraries(test_mobile_logic PRIVATE Qt6::Core Qt6::Test)

enable_testing()
add_test(NAME MobileLogic COMMAND test_mobile_logic)
