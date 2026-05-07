cmake_minimum_required(VERSION 3.16)
project(Prismalux_GUI VERSION 2.8 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTS "Compila la suite test_signal_lifetime" OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Qt6 con fallback a Qt5 ────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Widgets Network PrintSupport QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network Qt6::PrintSupport)
    message(STATUS "Prismalux GUI v${PROJECT_VERSION}: usando Qt6 ${Qt6_VERSION}")
else()
    find_package(Qt5 5.15 COMPONENTS Core Widgets Network PrintSupport REQUIRED)
    set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network Qt5::PrintSupport)
    message(STATUS "Prismalux GUI v${PROJECT_VERSION}: usando Qt5 ${Qt5_VERSION}")
endif()

# ── Backend C: solo hw_detect (rilevamento GPU cross-platform) ───────────
set(C_SRCS
    hw_detect.c
    qrcodegen.c
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
    pages/agenti_page_stt.cpp
    pages/agenti_page_tts.cpp
    pages/agenti_page_files.cpp
    pages/agenti_page_models.cpp
    pages/agenti_page_math.cpp
    pages/agenti_page_tools.cpp
    pages/agenti_page_pipeline.cpp
    pages/agenti_page_byzantine.cpp
    pages/agenti_page_bubbles.cpp
    pages/agenti_page_exec.cpp
    pages/agenti_page_format.cpp
    pages/agenti_page_stream.cpp
    pages/agenti_page_consiglio.cpp
    pages/agenti_page_knowledge.cpp
    pages/agenti_page_auto.cpp
    pages/agenti_page_feedback.cpp
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
    pages/manutenzione_page_bugs.cpp
    pages/manutenzione_page_cron.cpp
    pages/manutenzione_page_lan.cpp
    lan_server.cpp
    pages/impostazioni_page.cpp
    pages/simulatore_page.cpp
    pages/simulatore_algos.cpp
    pages/strumenti_page.cpp
    pages/grafico_canvas.cpp
    pages/grafico_page.cpp
    pages/programmazione_page.cpp
    pages/programmazione_page_git_repl.cpp
    pages/code_interpreter_widget.cpp
    pages/stable_diffusion_widget.cpp
    pages/matematica_page.cpp
    pages/ricerca_page.cpp
    pages/app_controller_page.cpp
    pages/opencode_page.cpp
    monitor_panel.cpp
    # Widget nuovi: ChatBubble, ChartWidget, FormulaParser, QrCodeWidget
    widgets/chat_bubble.cpp
    widgets/chart_widget.cpp
    widgets/formula_parser.cpp
    widgets/qr_code_widget.cpp
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

    # Sorgenti condivisi con l'app principale (escluso main.cpp + file C per qrcodegen/hw_detect)
    set(TEST_SHARED_SRCS ${CPP_SRCS})
    list(REMOVE_ITEM TEST_SHARED_SRCS main.cpp)
    list(APPEND TEST_SHARED_SRCS ${C_SRCS})

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

    # ── test_thinking_detect: _extractThinkingToken + classifyQuery + key-lock ──
    # Dipende solo da ai_client.cpp (per classifyQuery) + ai_thinking_detect.h.
    # NON include TEST_SHARED_SRCS: ai_client.cpp compila senza tutta la UI.
    add_executable(test_thinking_detect
        tests/test_thinking_detect.cpp
        ai_client.cpp
    )
    target_include_directories(test_thinking_detect PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_thinking_detect PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_thinking_detect PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_thinking_detect PRIVATE pthread m)
    endif()

    # ── test_ai_integration: AiClient REALE ↔ Ollama (richiede Ollama attivo) ──
    # Testa il percorso completo GUI: classifyQuery → AiChatParams → chat() →
    # JSON → Ollama → streaming → finished(). Nessun mock.
    # Override modello: PRISMALUX_TEST_MODEL=qwen2.5-coder:7b ./build_tests/test_ai_integration
    add_executable(test_ai_integration
        tests/test_ai_integration.cpp
        ai_client.cpp
    )
    target_include_directories(test_ai_integration PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_ai_integration PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_ai_integration PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_ai_integration PRIVATE pthread m)
    endif()

    # ── test_ai_stress: matrice parametri completa + stress + edge case ──
    # CAT-F: 24 combinazioni honesty×caveman×QueryType×think-capable (mock)
    # CAT-G: stress sequenziale — throttle, abort+retry, N=20, payload 12KB, storia 50 turni
    # CAT-H: edge case — 9 keyword temporali, system vuoto, busy guard, abort idle
    # CAT-I: qualità reale con Ollama — caveman, honesty, think, num_predict
    add_executable(test_ai_stress
        tests/test_ai_stress.cpp
        ai_client.cpp
    )
    target_include_directories(test_ai_stress PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_ai_stress PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_ai_stress PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_ai_stress PRIVATE pthread m)
    endif()

    # ── test_team_collab: pipeline multi-agente 4 fasi ──────────────────
    # CAT-J: sanity — nessuna rete, verifica costanti e helpers
    # CAT-K: pipeline sequenziale Architetto→Sviluppatore→Revisore→Tester
    # CAT-L: qualità output — class/def/test_ pattern, copertura minima
    # Modelli: PRISMALUX_TEST_MODEL_REASONING (default: gpt-oss:120b)
    #          PRISMALUX_TEST_MODEL_CODER     (default: qwen2.5-coder)
    add_executable(test_team_collab
        tests/test_team_collab.cpp
        ai_client.cpp
    )
    target_include_directories(test_team_collab PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_team_collab PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_team_collab PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_team_collab PRIVATE pthread m)
    endif()

    # ── test_lavoro_data: kOfferte, offerteFiltrate, tipoIcon, livLabel ──────
    add_executable(test_lavoro_data
        tests/test_lavoro_data.cpp
        pages/lavoro_data.cpp
    )
    target_include_directories(test_lavoro_data PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_lavoro_data PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lavoro_data PRIVATE pthread m)
    endif()

    # ── test_tutor_data: SubjectData × 6 soggetti, invarianti strutturali ───
    add_executable(test_tutor_data
        tests/test_tutor_data.cpp
        pages/tutor_data.cpp
    )
    target_include_directories(test_tutor_data PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_tutor_data PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_tutor_data PRIVATE pthread m)
    endif()

    # ── test_rag_engine_avanzato: JLT math, search edge, save/load, perf ───
    add_executable(test_rag_engine_avanzato
        tests/test_rag_engine_avanzato.cpp
        rag_engine.cpp
    )
    target_include_directories(test_rag_engine_avanzato PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_rag_engine_avanzato PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_rag_engine_avanzato PRIVATE pthread m)
    endif()

    # ── test_chat_history: CRUD sessioni, roundtrip, robustezza ──────────
    add_executable(test_chat_history
        tests/test_chat_history.cpp
        chat_history.cpp
    )
    target_include_directories(test_chat_history PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_chat_history PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_chat_history PRIVATE pthread m)
    endif()

    # ── test_simulatore_algos: BubbleSort+sorting+ricerca+BigO (50 test) ─
    add_executable(test_simulatore_algos
        tests/test_simulatore_algos.cpp
        tests/mock_ai_client.h
        pages/simulatore_page.cpp
        pages/simulatore_algos.cpp
        ai_client.cpp
        ${C_SRCS}
    )
    target_include_directories(test_simulatore_algos PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_simulatore_algos PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_simulatore_algos PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_simulatore_algos PRIVATE pthread m)
    endif()

    # ── test_formula_parser: FormulaParser (parser RD, eval, sample, tryExtract) ──
    add_executable(test_formula_parser
        tests/test_formula_parser.cpp
        widgets/formula_parser.cpp
    )
    target_include_directories(test_formula_parser PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_formula_parser PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_formula_parser PRIVATE pthread m)
    endif()

    # ── test_strumenti_rag: ragChunkText + ragExtractText + sysPromptForAction ──
    add_executable(test_strumenti_rag
        tests/test_strumenti_rag.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_strumenti_rag PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_strumenti_rag PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_strumenti_rag PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_strumenti_rag PRIVATE pthread m)
    endif()

    # ── test_theme_manager: ThemeManager singleton + lista temi + apply ──
    add_executable(test_theme_manager
        tests/test_theme_manager.cpp
        theme_manager.cpp
    )
    target_include_directories(test_theme_manager PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_theme_manager PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_theme_manager PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_theme_manager PRIVATE pthread m)
    endif()

    # ── test_hardware_monitor: hw_detect + HardwareMonitor thread ──
    add_executable(test_hardware_monitor
        tests/test_hardware_monitor.cpp
        hardware_monitor.cpp
        ${C_SRCS}
    )
    target_include_directories(test_hardware_monitor PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_hardware_monitor PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_hardware_monitor PRIVATE pthread m)
    endif()

    # ── test_manutenzione_cron: cronShouldRun + cronNextRun + detectConfigFmt ──
    add_executable(test_manutenzione_cron
        tests/test_manutenzione_cron.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_manutenzione_cron PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_manutenzione_cron PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_manutenzione_cron PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_manutenzione_cron PRIVATE pthread m)
    endif()

    # ── test_chat_history_stress: concorrenza 4 thread + durabilità ──
    add_executable(test_chat_history_stress
        tests/test_chat_history_stress.cpp
        chat_history.cpp
    )
    target_include_directories(test_chat_history_stress PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_chat_history_stress PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_chat_history_stress PRIVATE pthread m)
    endif()

    # ── test_matematica_page: parseSeq, detectPatternLocal, numbersFromText, widget ──
    add_executable(test_matematica_page
        tests/test_matematica_page.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_matematica_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_matematica_page PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_matematica_page PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_matematica_page PRIVATE pthread m)
    endif()

    # ── test_qr_code_widget: QrCodeWidget generazione, rendering, dialog APK ──
    add_executable(test_qr_code_widget
        tests/test_qr_code_widget.cpp
        widgets/qr_code_widget.cpp
        qrcodegen.c
    )
    target_include_directories(test_qr_code_widget PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_qr_code_widget PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_qr_code_widget PRIVATE pthread m)
    endif()

    # ── test_hw_detect_amd: rilevamento AMD via DRM sysfs senza ROCm ──
    add_executable(test_hw_detect_amd
        tests/test_hw_detect_amd.cpp
        ${C_SRCS}
    )
    target_include_directories(test_hw_detect_amd PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_hw_detect_amd PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_hw_detect_amd PRIVATE pthread m)
    endif()

    # ── test_knowledge_injection: readUserKnowledge, prependKnowledge, cache 30s ──
    add_executable(test_knowledge_injection
        tests/test_knowledge_injection.cpp
    )
    target_include_directories(test_knowledge_injection PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_knowledge_injection PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_knowledge_injection PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_knowledge_injection PRIVATE pthread m)
    endif()

    # ── test_agenti_pipeline: buildUserBubble, buildAgentBubble, buildLocalBubble, markdownToHtml ──
    add_executable(test_agenti_pipeline
        tests/test_agenti_pipeline.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_agenti_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agenti_pipeline PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_agenti_pipeline PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_agenti_pipeline PRIVATE pthread m)
    endif()

    # ── test_agents_config_dialog: struttura widget, numAgents, modeCombo, RAG condiviso ──
    add_executable(test_agents_config_dialog
        tests/test_agents_config_dialog.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_agents_config_dialog PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agents_config_dialog PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_agents_config_dialog PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_agents_config_dialog PRIVATE pthread m)
    endif()

    # ── test_lan_server: lifecycle TCP, porta, segnali, client connessi ──
    add_executable(test_lan_server
        tests/test_lan_server.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_lan_server PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lan_server PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_lan_server PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_lan_server PRIVATE pthread m)
    endif()

    add_executable(test_impostazioni_page
        tests/test_impostazioni_page.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_impostazioni_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_impostazioni_page PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_impostazioni_page PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_impostazioni_page PRIVATE pthread m)
    endif()

    add_executable(test_manutenzione_bugs
        tests/test_manutenzione_bugs.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_manutenzione_bugs PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_manutenzione_bugs PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_manutenzione_bugs PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_manutenzione_bugs PRIVATE pthread m)
    endif()

    add_executable(test_grafico
        tests/test_grafico.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_grafico PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_grafico PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_grafico PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_grafico PRIVATE pthread m)
    endif()

    add_executable(test_agenti_byzantine
        tests/test_agenti_byzantine.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_agenti_byzantine PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agenti_byzantine PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_agenti_byzantine PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_agenti_byzantine PRIVATE pthread m)
    endif()

    # ── test_agente_autonomo: ReAct loop, toggle UI, parsing tool call ──
    add_executable(test_agente_autonomo
        tests/test_agente_autonomo.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_agente_autonomo PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agente_autonomo PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_agente_autonomo PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_agente_autonomo PRIVATE pthread m)
    endif()

    add_executable(test_opencode_page
        tests/test_opencode_page.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_opencode_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_opencode_page PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_opencode_page PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_opencode_page PRIVATE pthread m)
    endif()

    add_executable(test_impara_quiz
        tests/test_impara_quiz.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_impara_quiz PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_impara_quiz PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_impara_quiz PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_impara_quiz PRIVATE pthread m)
    endif()

    add_executable(test_monitor_panel
        tests/test_monitor_panel.cpp
        tests/mock_ai_client.h
        ${TEST_SHARED_SRCS}
        ${C_SRCS}
    )
    target_include_directories(test_monitor_panel PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_monitor_panel PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_monitor_panel PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_monitor_panel PRIVATE pthread m)
    endif()

    enable_testing()
    add_test(NAME SignalLifetime        COMMAND test_signal_lifetime)
    add_test(NAME RagEngine            COMMAND test_rag_engine)
    add_test(NAME CodeUtils            COMMAND test_code_utils)
    add_test(NAME RandomTool           COMMAND test_random_tool)
    add_test(NAME LavoroPage           COMMAND test_lavoro_page)
    add_test(NAME AppController        COMMAND test_app_controller)
    add_test(NAME ProgrammazionePage   COMMAND test_programmazione_page)
    add_test(NAME ThinkingDetect       COMMAND test_thinking_detect)
    add_test(NAME AiIntegration        COMMAND test_ai_integration)
    add_test(NAME AiStress             COMMAND test_ai_stress)
    add_test(NAME TeamCollab           COMMAND test_team_collab)
    add_test(NAME LavoroData           COMMAND test_lavoro_data)
    add_test(NAME TutorData            COMMAND test_tutor_data)
    add_test(NAME RagEngineAvanzato    COMMAND test_rag_engine_avanzato)
    add_test(NAME ChatHistory          COMMAND test_chat_history)
    add_test(NAME SimulatoreAlgos      COMMAND test_simulatore_algos)
    add_test(NAME FormulaParser        COMMAND test_formula_parser)
    add_test(NAME StrumentiRag         COMMAND test_strumenti_rag)
    add_test(NAME ThemeManager         COMMAND test_theme_manager)
    add_test(NAME HardwareMonitor      COMMAND test_hardware_monitor)
    add_test(NAME ManutenzioneeCron    COMMAND test_manutenzione_cron)
    add_test(NAME ChatHistoryStress    COMMAND test_chat_history_stress)
    add_test(NAME MatematicaPage       COMMAND test_matematica_page)
    add_test(NAME QrCodeWidget        COMMAND test_qr_code_widget)
    add_test(NAME HwDetectAmd         COMMAND test_hw_detect_amd)
    add_test(NAME KnowledgeInjection      COMMAND test_knowledge_injection)
    add_test(NAME AgentiPipeline          COMMAND test_agenti_pipeline)
    add_test(NAME AgentsConfigDialog      COMMAND test_agents_config_dialog)
    add_test(NAME LanServer               COMMAND test_lan_server)
    add_test(NAME ImpostazioniPage        COMMAND test_impostazioni_page)
    add_test(NAME ManutenzioneeBugs       COMMAND test_manutenzione_bugs)
    add_test(NAME Grafico                 COMMAND test_grafico)
    add_test(NAME AgentiByzantine         COMMAND test_agenti_byzantine)
    add_test(NAME AgenteAutonomo          COMMAND test_agente_autonomo)
    add_test(NAME OpenCodePage            COMMAND test_opencode_page)
    add_test(NAME ImparaQuiz              COMMAND test_impara_quiz)
    add_test(NAME MonitorPanel            COMMAND test_monitor_panel)
    set_tests_properties(AiIntegration  PROPERTIES TIMEOUT 900)   # 15 min — CPU-only mistral lento
    set_tests_properties(AiStress       PROPERTIES TIMEOUT 1800)  # 30 min — 6 test reali + stress
    set_tests_properties(TeamCollab PROPERTIES TIMEOUT 2400)  # 40 min totali
    set_tests_properties(RagEngineAvanzato  PROPERTIES TIMEOUT 120)
    set_tests_properties(SimulatoreAlgos    PROPERTIES TIMEOUT 120)
    set_tests_properties(HardwareMonitor    PROPERTIES TIMEOUT 90)   # initTestCase + 2x thread hw_detect
    set_tests_properties(ChatHistoryStress  PROPERTIES TIMEOUT 60)
    # Serializza i test che usano Ollama reale: con -j4 girano in parallelo e saturano RAM/CPU
    set_tests_properties(AiIntegration AiStress TeamCollab
        PROPERTIES RESOURCE_LOCK ollama)
    set_tests_properties(LanServer PROPERTIES TIMEOUT 30)
    set_tests_properties(ImpostazioniPage   PROPERTIES TIMEOUT 30)
    set_tests_properties(ManutenzioneeBugs  PROPERTIES TIMEOUT 30)
    set_tests_properties(Grafico            PROPERTIES TIMEOUT 30)
    set_tests_properties(AgentiByzantine    PROPERTIES TIMEOUT 30)
    set_tests_properties(OpenCodePage       PROPERTIES TIMEOUT 30)
    set_tests_properties(ImparaQuiz         PROPERTIES TIMEOUT 30)
    set_tests_properties(MonitorPanel       PROPERTIES TIMEOUT 30)
    message(STATUS "Build test abilitata: signal_lifetime, rag_engine, code_utils, random_tool, lavoro_page, app_controller, programmazione_page, thinking_detect, ai_integration, ai_stress, team_collab, lavoro_data, tutor_data, rag_engine_avanzato, chat_history, simulatore_algos, formula_parser, strumenti_rag, theme_manager, hardware_monitor, manutenzione_cron, chat_history_stress, matematica_page, qr_code_widget, hw_detect_amd, knowledge_injection")
endif()

# ── CPack — Windows installer (NSIS) ─────────────────────────────────────
# Uso: cmake --build build --config Release && cpack --config build/CPackConfig.cmake
if(WIN32)
    install(TARGETS Prismalux_GUI
        RUNTIME DESTINATION .
        COMPONENT Runtime
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/themes/
        DESTINATION themes
        COMPONENT Runtime
    )
    # Le DLL Qt copiate da windeployqt (POST_BUILD) stanno nella build dir
    install(DIRECTORY ${CMAKE_BINARY_DIR}/
        DESTINATION .
        COMPONENT Runtime
        FILES_MATCHING PATTERN "*.dll"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/platforms/
        DESTINATION platforms
        COMPONENT Runtime
        FILES_MATCHING PATTERN "*.dll"
    )
    install(DIRECTORY ${CMAKE_BINARY_DIR}/imageformats/
        DESTINATION imageformats
        COMPONENT Runtime
        FILES_MATCHING PATTERN "*.dll"
    )

    set(CPACK_PACKAGE_NAME "Prismalux")
    set(CPACK_PACKAGE_VENDOR "wildlux")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Piattaforma AI locale — multi-agente, RAG, matematica, grafici")
    set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(CPACK_PACKAGE_VERSION_PATCH "0")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Prismalux")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/../LICENSE")

    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Prismalux ${PROJECT_VERSION}")
    set(CPACK_NSIS_PACKAGE_NAME "Prismalux")
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Prismalux.lnk' '$INSTDIR\\\\Prismalux_GUI.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Prismalux.lnk'")
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "Prismalux_GUI.exe")

    include(CPack)
endif()
