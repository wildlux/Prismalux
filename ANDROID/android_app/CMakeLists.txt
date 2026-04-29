cmake_minimum_required(VERSION 3.21)
project(PrismaluxMobile VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Qt6 con tutti i moduli necessari ─────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Network
    Bluetooth
    Multimedia
    MultimediaWidgets
)

set(QT_LIBS
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Bluetooth
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# ── Sorgenti ──────────────────────────────────────────────────────────────
set(SRCS
    main.cpp
    mainwindow.cpp
    ai_client.cpp
    rag_engine_simple.cpp
    pages/chat_page.cpp
    pages/camera_page.cpp
    pages/ble_page.cpp
    pages/settings_page.cpp
)

# ── Target ────────────────────────────────────────────────────────────────
qt_add_executable(PrismaluxMobile ${SRCS})

target_include_directories(PrismaluxMobile PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(PrismaluxMobile PRIVATE ${QT_LIBS})

target_compile_features(PrismaluxMobile PRIVATE cxx_std_17)

if(NOT MSVC)
    target_compile_options(PrismaluxMobile PRIVATE
        -Wall -Wextra -Wno-unused-parameter
    )
endif()

# ── Proprietà Android ─────────────────────────────────────────────────────
if(ANDROID)
    set_target_properties(PrismaluxMobile PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
        QT_ANDROID_MIN_SDK_VERSION    26          # Android 8.0 — BLE + Camera2
        QT_ANDROID_TARGET_SDK_VERSION 34          # Android 14
        QT_ANDROID_VERSION_CODE       1
        QT_ANDROID_VERSION_NAME       "1.0"
    )

    # Permessi dichiarati anche nel manifest; qui come fallback per Qt deploy
    set_property(TARGET PrismaluxMobile APPEND PROPERTY
        QT_ANDROID_EXTRA_LIBS
        # Aggiungere qui eventuali .so di terze parti
    )
endif()

# ── Risorsa QSS ───────────────────────────────────────────────────────────
# Copia lo stylesheet nella build dir per il caricamento runtime
if(NOT ANDROID)
    add_custom_command(TARGET PrismaluxMobile POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/style_mobile.qss
            $<TARGET_FILE_DIR:PrismaluxMobile>/style_mobile.qss
        COMMENT "Copio style_mobile.qss..."
    )
endif()

# Messaggio di riepilogo
message(STATUS "PrismaluxMobile v${PROJECT_VERSION}")
message(STATUS "  Android build: ${ANDROID}")
message(STATUS "  Qt6 Core:      ${Qt6Core_VERSION}")
