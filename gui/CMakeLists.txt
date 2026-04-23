cmake_minimum_required(VERSION 3.16)
project(Prismalux_GUI VERSION 2.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTS "Compila la suite test_signal_lifetime" OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Qt6 con fallback a Qt5 ────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Widgets Network QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network)
    message(STATUS "Prismalux GUI v${PROJECT_VERSION}: usando Qt6 ${Qt6_VERSION}")
else()
    find_package(Qt5 5.15 COMPONENTS Core Widgets Network REQUIRED)
    set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network)
    message(STATUS "Prismalux GUI v${PROJECT_VERSION}: usando Qt5 ${Qt5_VERSION}")
endif()

# ── Backend C: solo hw_detect (rilevamento GPU cross-platform) ───────────
set(C_SRCS
    hw_detect.c
)
set_source_files_properties(${C_SRCS} PROPERTIES LANGUAGE C)

# ── Sorgenti C++ Qt ──────────────────────────────────────────────────────
set(CPP_SRCS
    main.cpp
    mainwindow.cpp
    hardware_monitor.cpp
    ai_client.cpp
    theme_manager.cpp
    chat_history.cpp
    rag_engine.cpp
    pages/agenti_page.cpp
    pages/agenti_page_ui.cpp
    pages/agenti_page_voice.cpp
    pages/agenti_page_files.cpp
    pages/agenti_page_models.cpp
    pages/agenti_page_math.cpp
    pages/agenti_page_tools.cpp
    pages/agenti_page_pipeline.cpp
    pages/agenti_page_byzantine.cpp
    pages/agenti_page_bubbles.cpp
    pages/agenti_page_exec.cpp
    pages/agenti_page_stream.cpp
    pages/agenti_page_consiglio.cpp
    pages/agents_config_dialog.cpp
    pages/pratico_page.cpp
    pages/lavoro_data.cpp
    pages/lavoro_page.cpp
    pages/impara_page.cpp
    pages/quiz_page.cpp
    pages/oracolo_page.cpp
    pages/materie_page.cpp
    pages/tutor_data.cpp
    pages/personalizza_page.cpp
    pages/manutenzione_page.cpp
    pages/impostazioni_page.cpp
    pages/simulatore_page.cpp
    pages/simulatore_algos.cpp
    pages/strumenti_page.cpp
    pages/grafico_canvas.cpp
    pages/grafico_page.cpp
    pages/programmazione_page.cpp
    pages/programmazione_page_git_repl.cpp
    pages/matematica_page.cpp
    pages/app_controller_page.cpp
    monitor_panel.cpp
    # Widget nuovi: ChatBubble, ChartWidget, FormulaParser
    widgets/chat_bubble.cpp
    widgets/chart_widget.cpp
    widgets/formula_parser.cpp
    # Widget header-only con Q_OBJECT: vanno elencati per AUTOMOC
    widgets/spinner_widget.h
    widgets/status_badge.h
    widgets/chat_bubble.h
    widgets/chart_widget.h
    widgets/toggle_switch.h
    widgets/whisper_autosetup.h
    widgets/code_highlighter.h
    widgets/python_exec.h
)

add_executable(Prismalux_GUI ${CPP_SRCS} ${C_SRCS})

target_include_directories(Prismalux_GUI PRIVATE
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(Prismalux_GUI PRIVATE ${QT_LIBS})

# Percorso assoluto della root Prismalux (usato da personalizza_page per gcc command)
target_compile_definitions(Prismalux_GUI PRIVATE
    PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
)

if(WIN32)
    target_link_libraries(Prismalux_GUI PRIVATE ws2_32)
    target_compile_definitions(Prismalux_GUI PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        # M_PI e M_E sono disponibili con MinGW/GCC senza _USE_MATH_DEFINES,
        # ma la definizione esplicita garantisce compatibilità anche con MSVC.
        _USE_MATH_DEFINES
    )
    # _WIN32_WINNT rimosso: Qt6 lo definisce già — ridefinirlo causa warning con MinGW GCC
    # Nasconde la console su Windows (app grafica pura)
    set_target_properties(Prismalux_GUI PROPERTIES WIN32_EXECUTABLE TRUE)
    # Deploy DLL Qt dopo la build
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET Prismalux_GUI POST_BUILD
            COMMAND ${WINDEPLOYQT} $<TARGET_FILE:Prismalux_GUI>
            COMMENT "Copia DLL Qt con windeployqt...")
    endif()
else()
    target_link_libraries(Prismalux_GUI PRIVATE pthread m)
    target_compile_options(Prismalux_GUI PRIVATE -Wno-overflow)
endif()

# Copia la cartella themes/ accanto all'eseguibile dopo ogni build.
# I file .qss vengono letti da disco a runtime — nessun QRC embedded.
add_custom_command(TARGET Prismalux_GUI POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/themes
        $<TARGET_FILE_DIR:Prismalux_GUI>/themes
    COMMENT "Copio themes/ nella build dir..."
)

message(STATUS "Prismalux GUI: grafico_page include SmithPrime (idx 7) + MathConst-pi-e-Primi (idx 8)")

# ── Test suite: test_signal_lifetime ─────────────────────────────────────
# Testa 3 categorie di bug invisibili:
#   - Dangling Observer (token duplicato da doppia connessione)
#   - Signal Leakage    (token ricevuti da pagine in stato Idle)
#   - Invariant Violation (checkbox nascosti ≠ deselezionati)
#
# Build: cmake -B build_tests -DBUILD_TESTS=ON && cmake --build build_tests
# Run:   ./build_tests/test_signal_lifetime
if(BUILD_TESTS)
    if(Qt6_FOUND)
        find_package(Qt6 COMPONENTS Test REQUIRED)
        set(QT_TEST_LIB Qt6::Test)
    else()
        find_package(Qt5 COMPONENTS Test REQUIRED)
        set(QT_TEST_LIB Qt5::Test)
    endif()

    # Sorgenti condivisi con l'app principale (escluso main.cpp)
    set(TEST_SHARED_SRCS ${CPP_SRCS})
    list(REMOVE_ITEM TEST_SHARED_SRCS main.cpp)

    add_executable(test_signal_lifetime
        tests/test_signal_lifetime.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_signal_lifetime PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_signal_lifetime PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_signal_lifetime PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_signal_lifetime PRIVATE pthread m)
    endif()

    # ── test_rag_engine: solo rag_engine.cpp + Qt (nessuna dipendenza UI) ──
    add_executable(test_rag_engine
        tests/test_rag_engine.cpp
        rag_engine.cpp
    )
    target_include_directories(test_rag_engine PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_rag_engine PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_rag_engine PRIVATE pthread m)
    endif()

    # ── test_code_utils: extractPythonCode + _sanitizePyCode (static methods) ──
    # Richiede l'intera sorgente di agenti_page (metodi statici non separabili
    # senza il refactor A2). Stessa base di sorgenti di test_signal_lifetime.
    add_executable(test_code_utils
        tests/test_code_utils.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_code_utils PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_code_utils PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_code_utils PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_code_utils PRIVATE pthread m)
    endif()

    # ── test_random_tool: _inject_random da agenti_page_tools.cpp ──
    add_executable(test_random_tool
        tests/test_random_tool.cpp
        pages/agenti_page_tools.cpp
    )
    target_include_directories(test_random_tool PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_random_tool PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_random_tool PRIVATE pthread m)
    endif()

    # ── test_lavoro_page: LavoroPage isolation, filtri, DB integrità ──
    # 5 classi × N test: cross-contamination, filtri, modelli, DB, UI
    add_executable(test_lavoro_page
        tests/test_lavoro_page.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_lavoro_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lavoro_page PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_lavoro_page PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_lavoro_page PRIVATE pthread m)
    endif()

    # ── test_app_controller: AppControllerPage 100 test in 9 categorie ──
    add_executable(test_app_controller
        tests/test_app_controller.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_app_controller PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_app_controller PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_app_controller PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_app_controller PRIVATE pthread m)
    endif()

    # ── test_programmazione_page: isIntentionalError + parseNumbers + widget ──
    add_executable(test_programmazione_page
        tests/test_programmazione_page.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_programmazione_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_programmazione_page PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_programmazione_page PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_programmazione_page PRIVATE pthread m)
    endif()

    enable_testing()
    add_test(NAME SignalLifetime        COMMAND test_signal_lifetime)
    add_test(NAME RagEngine            COMMAND test_rag_engine)
    add_test(NAME CodeUtils            COMMAND test_code_utils)
    add_test(NAME RandomTool           COMMAND test_random_tool)
    add_test(NAME LavoroPage           COMMAND test_lavoro_page)
    add_test(NAME AppController        COMMAND test_app_controller)
    add_test(NAME ProgrammazionePage   COMMAND test_programmazione_page)
    message(STATUS "Build test abilitata: test_signal_lifetime, test_rag_engine, test_code_utils, test_random_tool, test_lavoro_page, test_programmazione_page")
endif()
