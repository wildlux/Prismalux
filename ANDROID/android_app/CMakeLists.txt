cmake_minimum_required(VERSION 3.21)
project(PrismaluxMobile VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Qt6 — moduli obbligatori ──────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network
)

set(QT_LIBS
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# ── Moduli opzionali ──────────────────────────────────────────────────────
find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth Multimedia MultimediaWidgets)
if(Qt6Bluetooth_FOUND)
    list(APPEND QT_LIBS Qt6::Bluetooth)
    add_compile_definitions(HAVE_BLE)
    message(STATUS "  Bluetooth: abilitato")
else()
    message(STATUS "  Bluetooth: non disponibile (stub)")
endif()
if(Qt6Multimedia_FOUND)
    list(APPEND QT_LIBS Qt6::Multimedia Qt6::MultimediaWidgets)
    add_compile_definitions(HAVE_MULTIMEDIA)
    message(STATUS "  Multimedia: abilitato")
else()
    message(STATUS "  Multimedia: non disponibile (stub)")
endif()

# ── Sorgenti ──────────────────────────────────────────────────────────────
set(SRCS
    main.cpp
    mainwindow.cpp
    ai_client.cpp
    rag_engine_simple.cpp
    pages/chat_page.cpp
    pages/settings_page.cpp
)

if(Qt6Multimedia_FOUND)
    list(APPEND SRCS pages/camera_page.cpp)
endif()

if(Qt6Bluetooth_FOUND)
    list(APPEND SRCS pages/ble_page.cpp)
endif()

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
    # ── OpenSSL per Android (richiesto da QNetworkAccessManager) ──
    if(NOT ANDROID_OPENSSL_DIR)
        set(ANDROID_OPENSSL_DIR "$ENV{HOME}/Android/android_openssl-master/ssl_3/${ANDROID_ABI}")
    endif()
    if(EXISTS "${ANDROID_OPENSSL_DIR}/libssl_3.so")
        set_target_properties(PrismaluxMobile PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
            QT_ANDROID_MIN_SDK_VERSION    26
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_VERSION_CODE       1
            QT_ANDROID_VERSION_NAME       "1.0"
            QT_ANDROID_EXTRA_LIBS
                "${ANDROID_OPENSSL_DIR}/libssl_3.so;${ANDROID_OPENSSL_DIR}/libcrypto_3.so"
        )
        message(STATUS "  OpenSSL ARM64: ${ANDROID_OPENSSL_DIR}")
    else()
        set_target_properties(PrismaluxMobile PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
            QT_ANDROID_MIN_SDK_VERSION    26
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_VERSION_CODE       1
            QT_ANDROID_VERSION_NAME       "1.0"
        )
        message(WARNING "  OpenSSL non trovato in ${ANDROID_OPENSSL_DIR} — rete HTTPS non disponibile")
    endif()
endif()

# ── Risorsa QSS (solo host) ───────────────────────────────────────────────
if(NOT ANDROID)
    add_custom_command(TARGET PrismaluxMobile POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/style_mobile.qss
            $<TARGET_FILE_DIR:PrismaluxMobile>/style_mobile.qss
        COMMENT "Copio style_mobile.qss..."
    )
endif()

message(STATUS "PrismaluxMobile v${PROJECT_VERSION}")
message(STATUS "  Android build: ${ANDROID}")
message(STATUS "  Qt6 Core:      ${Qt6Core_VERSION}")
