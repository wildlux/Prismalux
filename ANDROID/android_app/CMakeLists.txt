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
find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth Multimedia MultimediaWidgets WebSockets Pdf TextToSpeech)
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
if(Qt6Pdf_FOUND)
    list(APPEND QT_LIBS Qt6::Pdf)
    add_compile_definitions(HAVE_QT_PDF)
    message(STATUS "  PDF: abilitato (QPdfDocument)")
else()
    message(STATUS "  PDF: non disponibile (solo .txt/.md)")
endif()
if(Qt6WebSockets_FOUND)
    list(APPEND QT_LIBS Qt6::WebSockets)
    add_compile_definitions(HAVE_OBS_WS)
    message(STATUS "  WebSockets: abilitato (OBS control)")
else()
    message(STATUS "  WebSockets: non disponibile (OBS stub)")
endif()
if(Qt6TextToSpeech_FOUND)
    list(APPEND QT_LIBS Qt6::TextToSpeech)
    add_compile_definitions(HAVE_TTS)
    message(STATUS "  TextToSpeech: abilitato (lettura ad alta voce)")
else()
    message(STATUS "  TextToSpeech: non disponibile")
endif()

# ── llama.cpp — LLM locale (solo Android) ────────────────────────────────────
# Vulkan OFF: NDK v26.1 ha Vulkan-Hpp headers con duplicati pervasivi (bug NDK).
# NEON puro su Cortex-A76 (aarch64): ~8-12 tok/s su Qwen2-1.5B Q4_K_M — usabile.
if(ANDROID)
    include(FetchContent)
    FetchContent_Declare(
        llama_cpp
        GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
        GIT_TAG        b9181          # pinnato per riproducibilità
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    set(GGML_VULKAN          OFF CACHE BOOL "" FORCE)
    set(GGML_OPENMP          OFF CACHE BOOL "" FORCE)
    set(GGML_BLAS            OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(llama_cpp)
    if(TARGET llama)
        add_compile_definitions(HAVE_LOCAL_LLM)
        list(APPEND QT_LIBS llama)
        # posix_madvise assente su Android NDK: inietta il wrapper via -include
        foreach(_t llama ggml ggml-base ggml-cpu)
            if(TARGET ${_t})
                target_compile_options(${_t} PRIVATE
                    "-include${CMAKE_CURRENT_SOURCE_DIR}/android_compat.h")
            endif()
        endforeach()
        message(STATUS "  llama.cpp: abilitato (NEON arm64, Vulkan disabilitato)")
    else()
        message(WARNING "  llama.cpp: target 'llama' non trovato dopo FetchContent")
    endif()
endif()

# ── ZXing-cpp — decodifica QR dal vivo (solo se Multimedia presente) ──────
if(Qt6Multimedia_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        ZXingCpp
        GIT_REPOSITORY https://github.com/zxing-cpp/zxing-cpp.git
        GIT_TAG        v2.2.1
        GIT_SHALLOW    TRUE
    )
    set(ZXING_READERS  ON  CACHE BOOL "" FORCE)
    set(ZXING_WRITERS  OFF CACHE BOOL "" FORCE)
    set(ZXING_C_API    OFF CACHE BOOL "" FORCE)
    set(ZXING_USE_BUNDLED_ZINT OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(ZXingCpp)
    if(TARGET ZXing::ZXing)
        list(APPEND QT_LIBS ZXing::ZXing)
        add_compile_definitions(HAVE_ZXING)
        # FetchContent builds don't install to ZXing/ prefix — expose core/src directly
        get_target_property(_zxing_inc ZXing::ZXing INTERFACE_INCLUDE_DIRECTORIES)
        if(NOT _zxing_inc)
            set(_zxing_inc "${zxingcpp_SOURCE_DIR}/core/src")
        endif()
        list(APPEND _EXTRA_INCS "${_zxing_inc}")
        message(STATUS "  ZXing-cpp: abilitato (QR scanner, inc=${_zxing_inc})")
    else()
        message(STATUS "  ZXing-cpp: non disponibile (QR scanner disabilitato)")
    endif()
endif()

# ── Sorgenti ──────────────────────────────────────────────────────────────
set(SRCS
    main.cpp
    mainwindow.cpp
    ai_client.cpp
    thermal_monitor.cpp
    local_llm_client.cpp
    rag_engine_simple.cpp
    pages/chat_page.cpp
    pages/studio_page.cpp
    pages/quiz_ccna_db.cpp
    pages/lavoro_page.cpp
    pages/obs_page.cpp
    pages/misure_page.cpp
    pages/settings_page.cpp
    pages/info_page.cpp
    pages/mcpaddons_page.cpp
    pages/audio_page.cpp
)

if(Qt6Multimedia_FOUND)
    list(APPEND SRCS pages/camera_page.cpp)
    list(APPEND SRCS pages/qr_scanner_dialog.cpp)
endif()

if(Qt6Bluetooth_FOUND)
    list(APPEND SRCS pages/ble_page.cpp)
endif()

# ── Target ────────────────────────────────────────────────────────────────
qt_add_executable(PrismaluxMobile ${SRCS})

target_include_directories(PrismaluxMobile PRIVATE ${CMAKE_SOURCE_DIR} ${_EXTRA_INCS})

target_link_libraries(PrismaluxMobile PRIVATE ${QT_LIBS} z)

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

# ── Risorsa QSS — embeddato nel binario via Qt resource (funziona su Android) ──
qt_add_resources(PrismaluxMobile "qss_res"
    PREFIX "/"
    FILES
        style_mobile.qss
        images/prismalux_logo.png
)

# Copia anche accanto all'exe per debug desktop (utile per editing live del QSS)
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
