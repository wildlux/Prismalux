cmake_minimum_required(VERSION 3.21)
project(PrismaluxMobile VERSION 3.0 LANGUAGES C CXX)

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
find_package(Qt6 OPTIONAL_COMPONENTS Bluetooth Multimedia MultimediaWidgets WebSockets Pdf TextToSpeech Sql Positioning)
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
if(Qt6Sql_FOUND)
    list(APPEND QT_LIBS Qt6::Sql)
    add_compile_definitions(HAVE_SQL)
    message(STATUS "  Sql/SQLite: abilitato (quiz DB locale)")
else()
    message(STATUS "  Sql/SQLite: non disponibile (quiz usa DB in-memory)")
endif()
if(Qt6Positioning_FOUND)
    list(APPEND QT_LIBS Qt6::Positioning)
    add_compile_definitions(HAVE_POSITIONING)
    message(STATUS "  Positioning: abilitato (GPS Carta Astrale)")
else()
    message(STATUS "  Positioning: non disponibile (GPS disabilitato)")
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
    pages/impara_page.cpp
    pages/ricerca_page.cpp
    pages/matematica_page.cpp
    pages/sintetizzatore_page.cpp
    pages/assistente_page.cpp
    pages/oracle_page.cpp
    pages/hermes_page.cpp
    pages/file_ai_page.cpp
    pages/finanza_page.cpp
    pages/simulatore_page.cpp
    pages/wan_client_page.cpp
    pages/security_page.cpp
    pages/multi_agent_page.cpp
    pages/chart_page.cpp
    pages/opencode_page.cpp
    pages/pipeline_page.cpp
    pages/sfida_page.cpp
    pages/cron_page.cpp
    pages/editor_page.cpp
    pages/ricerca_mob_page.cpp
    pages/collab_page.cpp
    pages/appcontroller_page.cpp
    graph_memory_mobile.cpp
)

if(Qt6Multimedia_FOUND)
    list(APPEND SRCS pages/camera_page.cpp)
    list(APPEND SRCS pages/qr_scanner_dialog.cpp)
endif()

if(Qt6Bluetooth_FOUND)
    list(APPEND SRCS pages/ble_page.cpp)
    # qrcodegen per QR condivisione chiave BLE (onShareKey)
    list(APPEND SRCS ${CMAKE_SOURCE_DIR}/../../gui/qrcodegen.c)
    list(APPEND _EXTRA_INCS "${CMAKE_SOURCE_DIR}/../../gui")
endif()

# ── Form factor ───────────────────────────────────────────────────────────
# Imposta PRISMALUX_FORM_FACTOR_TABLET=ON nella riga cmake per build tablet:
#   cmake .. -DPRISMALUX_FORM_FACTOR_TABLET=ON
# Default: smartphone.
option(PRISMALUX_FORM_FACTOR_TABLET "Build per tablet (NavRail + split-panel)" OFF)
if(PRISMALUX_FORM_FACTOR_TABLET)
    add_compile_definitions(PRISMALUX_FORM_FACTOR_TABLET)
    message(STATUS "  Form factor: TABLET (NavRail + split-panel)")
else()
    add_compile_definitions(PRISMALUX_FORM_FACTOR_SMARTPHONE)
    message(STATUS "  Form factor: SMARTPHONE (hamburger drawer)")
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
    # ── OpenSSL per Android (rete HTTPS + cifratura BLE AES-256-GCM) ──
    if(NOT ANDROID_OPENSSL_DIR)
        set(ANDROID_OPENSSL_DIR "$ENV{HOME}/Android/android_openssl-master/ssl_3/${ANDROID_ABI}")
    endif()

    set(_OPENSSL_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../../../Android/android_openssl-master/ssl_3/include")
    if(NOT EXISTS "${_OPENSSL_INCLUDE}/openssl/evp.h")
        # tenta percorso assoluto derivato da ANDROID_OPENSSL_DIR
        get_filename_component(_OPENSSL_INCLUDE "${ANDROID_OPENSSL_DIR}/../include" ABSOLUTE)
    endif()

    if(EXISTS "${ANDROID_OPENSSL_DIR}/libssl_3.so")
        # Link statico libcrypto per cifratura AES-256-GCM nei payload BLE
        set(_OPENSSL_CRYPTO_STATIC "${ANDROID_OPENSSL_DIR}/libcrypto.a")
        if(NOT EXISTS "${_OPENSSL_CRYPTO_STATIC}")
            set(_OPENSSL_CRYPTO_STATIC "${ANDROID_OPENSSL_DIR}/libcrypto_3.so")
        endif()
        target_link_libraries(PrismaluxMobile PRIVATE "${_OPENSSL_CRYPTO_STATIC}")
        if(EXISTS "${_OPENSSL_INCLUDE}/openssl/evp.h")
            target_include_directories(PrismaluxMobile PRIVATE "${_OPENSSL_INCLUDE}")
        endif()
        target_compile_definitions(PrismaluxMobile PRIVATE HAVE_OPENSSL_CRYPTO)
        message(STATUS "  OpenSSL ARM64: ${ANDROID_OPENSSL_DIR} (BLE crypto: AES-256-GCM)")

        set_target_properties(PrismaluxMobile PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
            QT_ANDROID_MIN_SDK_VERSION    26
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_VERSION_CODE       30
            QT_ANDROID_VERSION_NAME       "3.0"
            QT_ANDROID_EXTRA_LIBS
                "${ANDROID_OPENSSL_DIR}/libssl_3.so;${ANDROID_OPENSSL_DIR}/libcrypto_3.so"
        )
    else()
        message(WARNING "  OpenSSL non trovato in ${ANDROID_OPENSSL_DIR} — BLE usa fallback HMAC-XOR")
        set_target_properties(PrismaluxMobile PROPERTIES
            QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_SOURCE_DIR}/android"
            QT_ANDROID_MIN_SDK_VERSION    26
            QT_ANDROID_TARGET_SDK_VERSION 34
            QT_ANDROID_VERSION_CODE       30
            QT_ANDROID_VERSION_NAME       "3.0"
        )
    endif()
endif()

# ── OpenSSL desktop (facoltativo: cifratura BLE anche in debug desktop) ──
if(NOT ANDROID)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_link_libraries(PrismaluxMobile PRIVATE OpenSSL::Crypto)
        target_compile_definitions(PrismaluxMobile PRIVATE HAVE_OPENSSL_CRYPTO)
        message(STATUS "  OpenSSL desktop: trovato (BLE crypto: AES-256-GCM)")
    else()
        message(STATUS "  OpenSSL desktop: non trovato (BLE usa fallback HMAC-XOR)")
    endif()
endif()

# ── Risorsa QSS — embeddato nel binario via Qt resource (funziona su Android) ──
qt_add_resources(PrismaluxMobile "qss_res"
    PREFIX "/"
    FILES
        style_mobile.qss
        images/prismalux_logo.png
        themes_mobile/dark_cyan.qss
        themes_mobile/neon.qss
        themes_mobile/hacker.qss
        themes_mobile/dark_purple.qss
        themes_mobile/dark_sunset.qss
        themes_mobile/dark_ocean.qss
        themes_mobile/venom_red.qss
        themes_mobile/dark_amber.qss
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
