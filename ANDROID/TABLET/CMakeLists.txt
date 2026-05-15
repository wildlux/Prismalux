cmake_minimum_required(VERSION 3.21)
project(PrismaluxTablet VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Form factor ───────────────────────────────────────────────────────────────
# TABLET = side navigation rail, split-panel layout, font 13pt
add_compile_definitions(PRISMALUX_FORM_FACTOR_TABLET)

# ── Sorgenti condivise con android_app ───────────────────────────────────────
set(SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../android_app")

# ── Qt6 ──────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)
set(QT_LIBS Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network)

find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth Multimedia MultimediaWidgets WebSockets)
if(Qt6Bluetooth_FOUND)
    list(APPEND QT_LIBS Qt6::Bluetooth)
    add_compile_definitions(HAVE_BLE)
endif()
if(Qt6Multimedia_FOUND)
    list(APPEND QT_LIBS Qt6::Multimedia Qt6::MultimediaWidgets)
    add_compile_definitions(HAVE_MULTIMEDIA)
endif()
if(Qt6WebSockets_FOUND)
    list(APPEND QT_LIBS Qt6::WebSockets)
    add_compile_definitions(HAVE_OBS_WS)
endif()

# ── Sorgenti ─────────────────────────────────────────────────────────────────
set(SRCS
    ${SHARED}/main.cpp
    ${SHARED}/mainwindow.cpp
    ${SHARED}/ai_client.cpp
    ${SHARED}/rag_engine_simple.cpp
    ${SHARED}/pages/chat_page.cpp
    ${SHARED}/pages/studio_page.cpp
    ${SHARED}/pages/lavoro_page.cpp
    ${SHARED}/pages/obs_page.cpp
    ${SHARED}/pages/misure_page.cpp
    ${SHARED}/pages/settings_page.cpp
    tablet_nav_rail.cpp
)
if(Qt6Multimedia_FOUND)
    list(APPEND SRCS ${SHARED}/pages/camera_page.cpp)
endif()
if(Qt6Bluetooth_FOUND)
    list(APPEND SRCS ${SHARED}/pages/ble_page.cpp)
endif()

# ── Risorse ──────────────────────────────────────────────────────────────────
qt_add_resources(SRCS "${SHARED}/resources.qrc" OPTIONAL)

# ── Target ───────────────────────────────────────────────────────────────────
qt_add_executable(PrismaluxTablet ${SRCS})

target_include_directories(PrismaluxTablet PRIVATE ${SHARED} ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(PrismaluxTablet PRIVATE ${QT_LIBS})

# ── Android ──────────────────────────────────────────────────────────────────
if(ANDROID)
    set_target_properties(PrismaluxTablet PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${SHARED}/android"
        QT_ANDROID_MIN_SDK_VERSION    26
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
endif()
