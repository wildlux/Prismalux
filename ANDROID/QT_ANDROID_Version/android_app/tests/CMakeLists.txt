cmake_minimum_required(VERSION 3.16)
project(PrismaluxMobileTests)

set(CMAKE_CXX_STANDARD 17)

# Usa Qt6 di sistema se Qt6_DIR non è specificato esplicitamente
if(NOT DEFINED Qt6_DIR)
    set(Qt6_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt6")
endif()

# Core + Test obbligatori; Sql opzionale (ChatPage persistenza SQLite)
find_package(Qt6 REQUIRED COMPONENTS Core Test)
find_package(Qt6 OPTIONAL_COMPONENTS Sql)

qt6_add_executable(test_mobile_logic test_mobile_logic.cpp)
set_target_properties(test_mobile_logic PROPERTIES AUTOMOC ON)

set(_TEST_LIBS Qt6::Core Qt6::Test)
if(Qt6Sql_FOUND)
    list(APPEND _TEST_LIBS Qt6::Sql)
    target_compile_definitions(test_mobile_logic PRIVATE HAVE_SQL)
    message(STATUS "  test_mobile_logic: Qt6::Sql trovato — suite ChatDb abilitata")
else()
    message(STATUS "  test_mobile_logic: Qt6::Sql non trovato — suite ChatDb saltata")
endif()

target_link_libraries(test_mobile_logic PRIVATE ${_TEST_LIBS})

enable_testing()
add_test(NAME MobileLogic COMMAND test_mobile_logic)
