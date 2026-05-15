cmake_minimum_required(VERSION 3.21)
project(PrismaluxSmartphone VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Form factor ───────────────────────────────────────────────────────────────
# SMARTPHONE = bottom navigation, full-screen single panel, font +2pt
add_compile_definitions(PRISMALUX_FORM_FACTOR_SMARTPHONE)

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
qt_add_executable(PrismaluxSmartphone ${SRCS})

target_include_directories(PrismaluxSmartphone PRIVATE ${SHARED})

target_link_libraries(PrismaluxSmartphone PRIVATE ${QT_LIBS})

# ── Android ──────────────────────────────────────────────────────────────────
if(ANDROID)
    set_target_properties(PrismaluxSmartphone PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${SHARED}/android"
        QT_ANDROID_MIN_SDK_VERSION    26
        QT_ANDROID_TARGET_SDK_VERSION 34
    )
endif()

# ── Build: cmake -B build-smartphone -DCMAKE_TOOLCHAIN_FILE=<Qt>/lib/cmake/Qt6/qt.toolchain.cmake
#           -DQT_ANDROID_SDK_ROOT=$ANDROID_SDK_ROOT -DANDROID_ABI=arm64-v8a
#           -DANDROID_PLATFORM=android-26
#    make -C build-smartphone apk
