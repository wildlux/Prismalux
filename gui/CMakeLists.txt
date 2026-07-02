cmake_minimum_required(VERSION 3.16)
project(Prismalux_GUI VERSION 3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTS        "Compila la suite test_signal_lifetime" OFF)
option(ENABLE_SANITIZERS  "Abilita AddressSanitizer + UBSanitizer (GCC/Clang)" OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Qt6 con fallback a Qt5 ────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Widgets Network PrintSupport Concurrent QUIET)
if(Qt6_FOUND)
    set(USE_QT6 TRUE)  # salvato prima che find_package opzionali resettino Qt6_FOUND
    set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network Qt6::PrintSupport Qt6::Concurrent)
    message(STATUS "Prismalux GUI v${PROJECT_VERSION}: usando Qt6 ${Qt6_VERSION}")
else()
    find_package(Qt5 5.15 COMPONENTS Core Widgets Network PrintSupport REQUIRED)
    set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network Qt5::PrintSupport)
    message(STATUS "Prismalux GUI v${PROJECT_VERSION}: usando Qt5 ${Qt5_VERSION}")
endif()

# ── Qt6::Sql — database SQLite (quiz CCNA) ───────────────────────────────
if(Qt6_FOUND)
    find_package(Qt6 COMPONENTS Sql QUIET)
    if(Qt6Sql_FOUND)
        list(APPEND QT_LIBS Qt6::Sql)
        add_compile_definitions(HAVE_QT_SQL)
        message(STATUS "Prismalux GUI: Qt6::Sql trovato — quiz CCNA SQLite abilitato")
    else()
        message(STATUS "Prismalux GUI: Qt6::Sql non trovato — quiz CCNA disabilitato")
    endif()
endif()

# ── Qt6Multimedia — rilevamento microfono reale (QAudioSource) ────────────
if(Qt6_FOUND)
    find_package(Qt6 COMPONENTS Multimedia QUIET)
    if(Qt6Multimedia_FOUND)
        list(APPEND QT_LIBS Qt6::Multimedia)
        add_compile_definitions(HAVE_QT_MULTIMEDIA)
        message(STATUS "Prismalux GUI: Qt6::Multimedia trovato — livello microfono reale abilitato")
    else()
        message(STATUS "Prismalux GUI: Qt6::Multimedia non trovato — livello microfono disabilitato")
    endif()
endif()

# ── Qt6::WebEngineWidgets — rendering LaTeX via KaTeX (opzionale) ────────
if(Qt6_FOUND)
    find_package(Qt6 COMPONENTS WebEngineWidgets QUIET)
    if(Qt6WebEngineWidgets_FOUND)
        list(APPEND QT_LIBS Qt6::WebEngineWidgets)
        add_compile_definitions(HAVE_WEBENGINE_WIDGETS)
        message(STATUS "Prismalux GUI: Qt6::WebEngineWidgets trovato — rendering LaTeX KaTeX abilitato")
    else()
        message(STATUS "Prismalux GUI: Qt6::WebEngineWidgets non trovato — LaTeX usa fallback QTextEdit")
    endif()
endif()

# ── Qt6::Svg — rendering SVG per emoji OpenMoji nel TriModeButton ────────
if(Qt6_FOUND)
    find_package(Qt6 COMPONENTS Svg QUIET)
    if(Qt6Svg_FOUND)
        list(APPEND QT_LIBS Qt6::Svg)
        add_compile_definitions(HAVE_SVG)
        message(STATUS "Prismalux GUI: Qt6::Svg trovato — emoji OpenMoji SVG abilitato")
    else()
        message(STATUS "Prismalux GUI: Qt6::Svg non trovato — emoji usa font di sistema")
    endif()
endif()

# ── QKeychain — salvataggio sicuro token LAN (opzionale) ─────────────────
# Ricerca cmake config (richiede libqt6keychain-dev su Debian/Ubuntu).
# Se non trovato, si usa il fallback file-based (0600) già presente.
find_package(Qt6Keychain QUIET
    NAMES Qt6Keychain qt6keychain qtkeychain
    HINTS /usr/lib/cmake /usr/share/cmake /usr/local/lib/cmake
)
if(Qt6Keychain_FOUND OR Qt6keychain_FOUND OR qtkeychain_FOUND)
    if(TARGET Qt6::Keychain)
        list(APPEND QT_LIBS Qt6::Keychain)
        add_compile_definitions(HAVE_QKEYCHAIN)
        message(STATUS "Prismalux GUI: QKeychain trovato — token LAN salvato nel keyring di sistema")
    elseif(TARGET qt6keychain)
        list(APPEND QT_LIBS qt6keychain)
        add_compile_definitions(HAVE_QKEYCHAIN)
        message(STATUS "Prismalux GUI: Qt6Keychain trovato — token LAN salvato nel keyring di sistema")
    endif()
else()
    message(STATUS "Prismalux GUI: QKeychain non trovato — token LAN usa fallback file 0600 (~/.prismalux/lan_token.key)")
endif()

# ── ONNX Runtime — embedding locale senza rete (opzionale) ──────────────────
# Installa: sudo apt install libonnxruntime-dev
# Runtime già presente: libonnxruntime1.23 (~= /usr/lib/x86_64-linux-gnu/libonnxruntime.so.1.23)
find_path(ONNX_INCLUDE_DIR
    NAMES onnxruntime/core/session/onnxruntime_cxx_api.h
          onnxruntime_cxx_api.h
    HINTS /usr/include /usr/local/include
)
find_library(ONNX_LIB
    NAMES onnxruntime
    HINTS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
)
if(ONNX_INCLUDE_DIR AND ONNX_LIB)
    add_compile_definitions(HAVE_ONNX)
    message(STATUS "Prismalux GUI: ONNX Runtime trovato — embedding locale abilitato (${ONNX_LIB})")
else()
    message(STATUS "Prismalux GUI: ONNX Runtime non trovato — usa 'sudo apt install libonnxruntime-dev' per abilitarlo")
endif()

# ── Backend C: hw_detect (GPU cross-platform) + blhm_engine (POSIX thread pool) ──
set(C_SRCS
    hw_detect.c
    qrcodegen.c
    blhm_engine.c
)
set_source_files_properties(${C_SRCS} PROPERTIES LANGUAGE C)

# ── Sorgenti C++ Qt ──────────────────────────────────────────────────────
set(CPP_SRCS
    main.cpp
    mainwindow.cpp
    mainwindow_slots.cpp
    hardware_monitor.cpp
    model_processor.cpp
    ai_client.cpp
    ai_memory.cpp
    theme_manager.cpp
    chat_history.cpp
    context_compressor.cpp
    rag_engine.cpp
    pages/main_ai.cpp
    pages/main_ai_ui.cpp
    pages/main_ai_slots.cpp
    pages/main_ai_panels.cpp
    pages/main_ai_stt.cpp
    pages/main_ai_tts.cpp
    pages/main_ai_files.cpp
    pages/main_ai_models.cpp
    pages/main_ai_math.cpp
    pages/main_ai_tools.cpp
    pages/main_ai_pipeline.cpp
    pages/main_ai_byzantine.cpp
    pages/main_ai_bubbles.cpp
    pages/main_ai_exec.cpp
    pages/main_ai_format.cpp
    pages/main_ai_stream.cpp
    pages/main_ai_advice.cpp
    pages/main_ai_knowledge.cpp
    pages/main_ai_hermes.cpp
    pages/main_ai_auto.cpp
    pages/main_ai_feedback.cpp
    pages/dialog_agents_config.cpp
    pages/main_finance.cpp
    pages/main_jobs_data.cpp
    pages/main_jobs.cpp
    pages/main_learn.cpp
    pages/main_quiz.cpp
    pages/main_quiz_ccna_db.cpp
    pages/main_oracle.cpp
    pages/main_subjects.cpp
    pages/main_learn_data.cpp
    pages/main_customize.cpp
    pages/main_customize_lora.cpp
    pages/main_app_controller_science.cpp
    pages/main_app_controller_modding.cpp
    pages/main_maintenance.cpp
    pages/main_maintenance_bugs.cpp
    pages/main_maintenance_cron.cpp
    pages/main_maintenance_lan.cpp
    lan_server.cpp
    lan_server_ai.cpp
    lan_server_compute.cpp
    lan_server_api.cpp
    lan_web.qrc
    emoji.qrc
    pages/settings_main.cpp
    pages/settings_model_profiles.cpp
    pages/settings_ai.cpp
    pages/settings_slots.cpp
    pages/settings_visual.cpp
    pages/settings_llm.cpp
    pages/settings_voice.cpp
    pages/settings_system.cpp
    pages/settings_other.cpp
    pages/settings_security.cpp
    pages/main_simulator.cpp
    pages/main_simulator_algos.cpp
    pages/main_sim_sorting.cpp
    pages/main_sim_graph.cpp
    pages/main_sim_misc.cpp
    pages/main_tools.cpp
    pages/main_lan_wan.cpp
    pages/main_lan_gns3.cpp
    pages/main_wan_persist.cpp
    pages/main_wan_server.cpp
    pages/main_wan_extra.cpp
    pages/main_sci_compute.cpp
    pages/main_sci_compute_ui.cpp
    pages/main_sci_protein.cpp
    pages/main_multimedia.cpp
    pages/main_tools_file.cpp
    pages/main_graph_canvas.cpp
    pages/main_graph_canvas_events.cpp
    pages/main_graph_canvas_2d.cpp
    pages/main_graph_canvas_3d.cpp
    pages/main_graph_canvas_domains.cpp
    pages/main_graph_canvas_series.cpp
    pages/main_graph_canvas_bars.cpp
    pages/main_graph_canvas_stats.cpp
    pages/main_graph_canvas_relational.cpp
    pages/main_graph.cpp
    pages/main_programming.cpp
    pages/main_programming_git_repl.cpp
    pages/main_programming_translitter.cpp
    pages/main_programming_vpn.cpp
    pages/main_programming_agentica.cpp
    pages/main_programming_reveng.cpp
    pages/main_programming_network.cpp
    pages/main_programming_coding.cpp
    pages/main_programming_driverkernel.cpp
    pages/main_programming_wiby.cpp
    pages/main_programming_reverse_usb.cpp
    pages/main_net_policy.cpp
    pages/main_net_subnet.cpp
    pages/widget_formula_builder.cpp
    pages/widget_code_interpreter.cpp
    pages/widget_coding_lab.cpp
    pages/widget_webdev.cpp
    pages/widget_ssh_manager.cpp
    pages/widget_solar_calc.cpp
    pages/widget_idro.cpp
    pages/widget_bike.cpp
    pages/widget_ram_calculator.cpp
    pages/widget_stable_diffusion.cpp
    pages/widget_synthesizer.cpp
    pages/widget_voice_cloner.cpp
    pages/main_math.cpp
    pages/main_math_solve.cpp
    pages/main_math_analisi.cpp
    pages/main_math_bool.cpp
    pages/main_multi_agent.cpp
    pages/main_distillazione.cpp
    graph_memory.cpp
    rag_graph.cpp
    log_bus.cpp
    pages/main_research.cpp
    pages/main_research_astrale.cpp
    pages/main_research_rag_tester.cpp
    pages/rab0l_canvas.cpp
    pages/widget_ai_script_base.cpp
    pages/widget_cytoscape.cpp
    pages/widget_rdkit.cpp
    pages/widget_bioconda.cpp
    pages/widget_avogadro.cpp
    pages/widget_rab0l.cpp
    pages/widget_blhm.cpp
    pages/main_utility.cpp
    pages/main_bioinformatica.cpp
    pages/main_app_controller.cpp
    pages/main_app_controller_blender.cpp
    pages/main_app_controller_freecad.cpp
    pages/main_app_controller_cloudcompare.cpp
    pages/main_app_controller_office.cpp
    pages/main_app_controller_electronics.cpp
    pages/main_app_controller_telegram.cpp
    pages/main_app_controller_whatsapp.cpp
    pages/main_app_controller_devagent.cpp
    pages/main_mcp_manager.cpp
    pages/main_security.cpp
    pages/main_opencode.cpp
    monitor_panel.cpp
    # Widget nuovi: ChatBubble, ChartWidget, FormulaParser, QrCodeWidget, NatalChartWidget
    widgets/chat_bubble.cpp
    widgets/chart_widget.cpp
    widgets/formula_parser.cpp
    widgets/qr_code_widget.cpp
    widgets/natal_chart_widget.cpp
    widgets/world_map_widget.cpp
    widgets/astro_calc.cpp
    widgets/onnx_embedder.cpp
    # Widget header-only con Q_OBJECT: vanno elencati per AUTOMOC
    widgets/spinner_widget.h
    widgets/status_badge.h
    widgets/chat_bubble.h
    widgets/chart_widget.h
    widgets/natal_chart_widget.h
    widgets/world_map_widget.h
    widgets/toggle_switch.h
    widgets/tri_mode_button.h
    widgets/whisper_autosetup.h
    widgets/code_highlighter.h
    widgets/python_exec.h
    widgets/ai_error_widget.h
    widgets/widget_dep_check.h
    widgets/widget_docker_update.h
    widgets/widget_python_update.h
    widgets/model_combo_box.h
    widgets/latex_view.h
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
    if(MSVC)
        # Fix C2733: Windows SDK 10.0.26100.0 (Server 2025 / windows-latest post-mar-2025)
        # dichiara _BitScanReverse64 come extern "C" in winnt.h DOPO che le intrinseche
        # MSVC l'hanno già dichiarata come C++ → errore di linkage.
        # /FI forza windows.h come prima inclusione in ogni TU: winnt.h viene elaborato
        # per primo e la dichiarazione extern "C" non entra in conflitto.
        # Alternativa robusta: -DCMAKE_SYSTEM_VERSION=10.0.22621.0 in cmake configure.
        target_compile_options(Prismalux_GUI PRIVATE /FIwindows.h)
    endif()
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
    if(APPLE)
        # Bundle macOS + deploy automatico framework Qt
        set_target_properties(Prismalux_GUI PROPERTIES
            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_BUNDLE_NAME      "Prismalux"
            MACOSX_BUNDLE_BUNDLE_VERSION   "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_IDENTIFIER       "com.prismalux.app"
            MACOSX_BUNDLE_COPYRIGHT        "2026 Paolo Lo Bello"
        )
        find_program(MACDEPLOYQT macdeployqt HINTS "${Qt6_DIR}/../../../bin")
        if(MACDEPLOYQT)
            add_custom_command(TARGET Prismalux_GUI POST_BUILD
                COMMAND ${MACDEPLOYQT}
                        "$<TARGET_BUNDLE_DIR:Prismalux_GUI>"
                        -verbose=1
                COMMENT "Copia framework Qt con macdeployqt...")
        endif()
    endif()
endif()

# ── ONNX Runtime link + include (se trovato) ────────────────────────────────
if(ONNX_INCLUDE_DIR AND ONNX_LIB)
    target_include_directories(Prismalux_GUI PRIVATE ${ONNX_INCLUDE_DIR})
    target_link_libraries(Prismalux_GUI PRIVATE ${ONNX_LIB})
endif()

# ── Sanitizers (ASAN + UBSAN): attiva con -DENABLE_SANITIZERS=ON ─────────
# Uso: cmake -B build_asan -DENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
#       cmake --build build_asan -j$(nproc)
#       ASAN_OPTIONS=halt_on_error=1 ./build_asan/Prismalux_GUI
if(ENABLE_SANITIZERS AND NOT MSVC)
    set(_SAN_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_compile_options(Prismalux_GUI PRIVATE ${_SAN_FLAGS})
    target_link_options(Prismalux_GUI    PRIVATE ${_SAN_FLAGS})
    message(STATUS "Prismalux GUI: ASAN+UBSAN abilitati")
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

# ── i18n — QTranslator ───────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS LinguistTools QUIET)
if(Qt6LinguistTools_FOUND)
    qt6_add_translation(QM_FILES
        i18n/prismalux_it.ts
        i18n/prismalux_en.ts
    )
    target_sources(Prismalux_GUI PRIVATE ${QM_FILES})
endif()

# ── clang-tidy (analisi statica): attiva con -DENABLE_CLANG_TIDY=ON ──────
# Richiede: sudo apt install clang-tidy-18
# Uso: cmake -B build_tidy -DENABLE_CLANG_TIDY=ON && cmake --build build_tidy --target tidy
option(ENABLE_CLANG_TIDY "Esegui clang-tidy su tutti i sorgenti" OFF)
if(ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE NAMES clang-tidy-20 clang-tidy-19 clang-tidy-18 clang-tidy-17 clang-tidy)
    if(CLANG_TIDY_EXE)
        message(STATUS "Prismalux GUI: clang-tidy trovato: ${CLANG_TIDY_EXE}")
        set_target_properties(Prismalux_GUI PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
    else()
        message(WARNING "ENABLE_CLANG_TIDY=ON ma clang-tidy non trovato. Installa: sudo apt install clang-tidy-18")
    endif()
endif()

# ── Test suite: test_signal_lifetime ─────────────────────────────────────
# Testa 3 categorie di bug invisibili:
#   - Dangling Observer (token duplicato da doppia connessione)
#   - Signal Leakage    (token ricevuti da pagine in stato Idle)
#   - Invariant Violation (checkbox nascosti ≠ deselezionati)
#
# Build: cmake -B Test/build_tests gui/ -DBUILD_TESTS=ON && cmake --build Test/build_tests
# Run:   ./Test/build_tests/test_signal_lifetime
if(BUILD_TESTS)
    if(USE_QT6)
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

    # ── OBJECT LIBRARY condivisa: compila UNA SOLA VOLTA i ~150 file di
    # TEST_SHARED_SRCS, riusata (linkata, non ricompilata) dai target
    # "pesanti" che oggi la duplicano interamente. Nome con prefisso
    # "test_" DELIBERATAMENTE, per essere intercettata dal loop ASAN
    # più sotto (foreach su BUILDSYSTEM_TARGETS filtrato per "^test_") —
    # non rinominare senza aggiornare anche quel loop.
    add_library(test_shared_objs OBJECT ${TEST_SHARED_SRCS})
    target_include_directories(test_shared_objs PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_shared_objs PRIVATE ${QT_LIBS})
    # PUBLIC: alcuni tests/test_X.cpp includono prismalux_paths.h
    # direttamente e leggono #ifdef PRISMALUX_ROOT — la define deve
    # propagarsi anche alla TU del test stesso, non solo ai file
    # compilati qui dentro.
    target_compile_definitions(test_shared_objs PUBLIC
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )

    # Gruppo minimo per target che includono solo ai_client.cpp:
    # rag_engine e onnx_embedder sono richiesti da ai_client.cpp per il linker.
    set(AI_CLIENT_SRCS
        ai_client.cpp
        rag_engine.cpp
        widgets/onnx_embedder.cpp
    )

    # ── OBJECT LIBRARY leggera per i target "AI_CLIENT_SRCS" (5 target,
    # priorità più bassa rispetto a test_shared_objs). Nome con prefisso
    # "test_" per lo stesso motivo del loop ASAN spiegato sopra.
    add_library(test_ai_client_objs OBJECT ${AI_CLIENT_SRCS})
    target_include_directories(test_ai_client_objs PUBLIC ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_ai_client_objs PRIVATE ${QT_LIBS})
    target_compile_definitions(test_ai_client_objs PUBLIC
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )

    add_executable(test_signal_lifetime
        tests/test_signal_lifetime.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_signal_lifetime PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_signal_lifetime PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
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
    )
    target_include_directories(test_code_utils PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_code_utils PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_code_utils PRIVATE pthread m)
    endif()

    # ── test_random_tool: _inject_random da ai_random_inject.cpp (puro, senza dep UI) ──
    add_executable(test_random_tool
        tests/test_random_tool.cpp
        pages/ai_random_inject.cpp
    )
    target_include_directories(test_random_tool PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_random_tool PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_random_tool PRIVATE pthread m)
    endif()

    # ── test_external_ai_import: parseFile() per formati OpenAI/Claude/generico ──
    add_executable(test_external_ai_import
        tests/test_external_ai_import.cpp
        chat_history.cpp
    )
    target_include_directories(test_external_ai_import PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_external_ai_import PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_external_ai_import PRIVATE pthread m)
    endif()

    # ── test_lavoro_page: LavoroPage isolation, filtri, DB integrità ──
    # 5 classi × N test: cross-contamination, filtri, modelli, DB, UI
    add_executable(test_lavoro_page
        tests/test_lavoro_page.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_lavoro_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lavoro_page PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lavoro_page PRIVATE pthread m)
    endif()

    # ── test_mcp_manager: McpManagerPage helper + costruzione ──
    add_executable(test_mcp_manager
        tests/test_mcp_manager.cpp
    )
    target_include_directories(test_mcp_manager PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_mcp_manager PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_mcp_manager PRIVATE pthread m)
    endif()

    # ── test_app_controller: AppControllerPage 100 test in 9 categorie ──
    add_executable(test_app_controller
        tests/test_app_controller.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_app_controller PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_app_controller PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_app_controller PRIVATE pthread m)
    endif()

    # ── test_programmazione_page: isIntentionalError + parseNumbers + widget ──
    add_executable(test_programmazione_page
        tests/test_programmazione_page.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_programmazione_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_programmazione_page PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_programmazione_page PRIVATE pthread m)
    endif()

    # ── test_ai_memory: AIMemory git-backed (CAT-A costruzione + CAT-B operazioni) ──
    add_executable(test_ai_memory
        tests/test_ai_memory.cpp
        ai_memory.cpp
        ${C_SRCS}
    )
    target_include_directories(test_ai_memory PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_ai_memory PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_ai_memory PRIVATE pthread m)
    endif()
    add_test(NAME AiMemory COMMAND test_ai_memory)

    # ── test_translitter: langFence helper, costruzione widget, kLangs unicità ──
    add_executable(test_translitter
        tests/test_translitter.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_translitter PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_translitter PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_translitter PRIVATE pthread m)
    endif()

    # ── test_thinking_detect: _extractThinkingToken + classifyQuery + key-lock ──
    add_executable(test_thinking_detect
        tests/test_thinking_detect.cpp
    )
    target_include_directories(test_thinking_detect PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_thinking_detect PRIVATE test_ai_client_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_thinking_detect PRIVATE pthread m)
    endif()

    # ── test_ai_integration: AiClient REALE ↔ Ollama (richiede Ollama attivo) ──
    # Testa il percorso completo GUI: classifyQuery → AiChatParams → chat() →
    # JSON → Ollama → streaming → finished(). Nessun mock.
    # Override modello: PRISMALUX_TEST_MODEL=qwen2.5-coder:7b ./build_tests/test_ai_integration
    add_executable(test_ai_integration
        tests/test_ai_integration.cpp
    )
    target_include_directories(test_ai_integration PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_ai_integration PRIVATE test_ai_client_objs ${QT_LIBS} ${QT_TEST_LIB})
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
    )
    target_include_directories(test_ai_stress PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_ai_stress PRIVATE test_ai_client_objs ${QT_LIBS} ${QT_TEST_LIB})
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
    )
    target_include_directories(test_team_collab PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_team_collab PRIVATE test_ai_client_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_team_collab PRIVATE pthread m)
    endif()

    # ── test_multi_agente_live: AgentiMultiPage + GraphMemory ↔ Ollama reale ──
    # CAT-A: costruzione (nessun Ollama) — CAT-B: decomposizione JSON — 30 s
    # CAT-C: esecuzione SubTask + nodo result in GraphMemory — 60 s
    # CAT-D: persistenza GraphMemory, toDot(), toJson() — 60 s
    # Override modello: PRISMALUX_TEST_MODEL=qwen2.5-coder:7b ./build_tests/test_multi_agente_live
    add_executable(test_multi_agente_live
        tests/test_multi_agente_live.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_multi_agente_live PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_multi_agente_live PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_multi_agente_live PRIVATE pthread m)
    endif()

    # ── test_lavoro_data: kOfferte, offerteFiltrate, tipoIcon, livLabel ──────
    add_executable(test_lavoro_data
        tests/test_lavoro_data.cpp
        pages/main_jobs_data.cpp
    )
    target_include_directories(test_lavoro_data PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_lavoro_data PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lavoro_data PRIVATE pthread m)
    endif()

    # ── test_tutor_data: SubjectData × 6 soggetti, invarianti strutturali ───
    add_executable(test_tutor_data
        tests/test_tutor_data.cpp
        pages/main_learn_data.cpp
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
        pages/main_simulator.cpp
        pages/main_simulator_algos.cpp
        pages/main_sim_sorting.cpp
        pages/main_sim_graph.cpp
        pages/main_sim_misc.cpp
        ${AI_CLIENT_SRCS}
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
    )
    target_include_directories(test_strumenti_rag PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_strumenti_rag PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
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

    # ── test_theme_manager_crash: fix ABRT Signal 6 (parent=nullptr) ──
    add_executable(test_theme_manager_crash
        tests/test_theme_manager_crash.cpp
        theme_manager.cpp
    )
    target_include_directories(test_theme_manager_crash PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_theme_manager_crash PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_theme_manager_crash PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_theme_manager_crash PRIVATE pthread m)
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
    )
    target_include_directories(test_manutenzione_cron PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_manutenzione_cron PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
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
    )
    target_include_directories(test_matematica_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_matematica_page PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_matematica_page PRIVATE pthread m)
        # GCC 15: #define private public nel test altera visibilità in <sstream>
        # -include forza <sstream> prima di qualsiasi macro — solo per C++, non C
        target_compile_options(test_matematica_page PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-include sstream>)
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
    )
    target_include_directories(test_agenti_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agenti_pipeline PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_agenti_pipeline PRIVATE pthread m)
    endif()

    # ── test_agents_config_dialog: struttura widget, numAgents, modeCombo, RAG condiviso ──
    add_executable(test_agents_config_dialog
        tests/test_agents_config_dialog.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_agents_config_dialog PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agents_config_dialog PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_agents_config_dialog PRIVATE pthread m)
    endif()

    # ── test_ai_client: classifyQuery, detectQueryDomain, SmartRouter, abort, mock HTTP 4xx/5xx ──
    add_executable(test_ai_client
        tests/test_ai_client.cpp
    )
    target_include_directories(test_ai_client PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_ai_client PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_ai_client PRIVATE pthread m)
    endif()

    # ── test_lan_wan_core: timingSafeEqual, token save/load, rate limiting, lifecycle ──
    add_executable(test_lan_wan_core
        tests/test_lan_wan_core.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_lan_wan_core PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lan_wan_core PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lan_wan_core PRIVATE pthread m)
        # GCC 15: #define private public nel test altera visibilità in <sstream>
        # -include forza <sstream> prima di qualsiasi macro — solo per C++, non C
        target_compile_options(test_lan_wan_core PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-include sstream>)
    endif()

    # ── test_mcp_integration: McpManagerPage struct/scan/JSON-RPC protocol + integrazione reale ──
    add_executable(test_mcp_integration
        tests/test_mcp_integration.cpp
    )
    target_include_directories(test_mcp_integration PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_mcp_integration PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_mcp_integration PRIVATE pthread m)
    endif()

    # ── test_lan_server: lifecycle TCP, porta, segnali, client connessi ──
    add_executable(test_lan_server
        tests/test_lan_server.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_lan_server PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lan_server PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lan_server PRIVATE pthread m)
    endif()

    # ── test_lan_server_endpoints: endpoint HTTP /knowledge e /apk ──
    add_executable(test_lan_server_endpoints
        tests/test_lan_server_endpoints.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_lan_server_endpoints PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lan_server_endpoints PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lan_server_endpoints PRIVATE pthread m)
    endif()

    # ── test_lan_server_queue: broadcastQueuePositions() coda multi-utente ──
    add_executable(test_lan_server_queue
        tests/test_lan_server_queue.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_lan_server_queue PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_lan_server_queue PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_lan_server_queue PRIVATE pthread m)
    endif()

    # ── test_wan_compute_tasks: WAN Compute LanWanPage — costruzione, protocollo TCP, sicurezza shell ──
    add_executable(test_wan_compute_tasks
        tests/test_wan_compute_tasks.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_wan_compute_tasks PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_wan_compute_tasks PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_wan_compute_tasks PRIVATE pthread m)
    endif()

    # ── test_sci_compute: SciComputePage — costruzione, isSafeToolName, isSafePath, taskTypes ──
    add_executable(test_sci_compute
        tests/test_sci_compute.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_sci_compute PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_sci_compute PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_sci_compute PRIVATE pthread m)
    endif()

    # ── test_sintetizzatore: Tono struct, OscoCanvas, SintetizzatoreWidget ──
    add_executable(test_sintetizzatore
        tests/test_sintetizzatore.cpp
    )
    target_include_directories(test_sintetizzatore PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_sintetizzatore PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_sintetizzatore PRIVATE pthread m)
    endif()

    # ── test_stt_whisper: SttWhisper paths, availability, transcribe() ──
    add_executable(test_stt_whisper
        tests/test_stt_whisper.cpp
    )
    target_include_directories(test_stt_whisper PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_stt_whisper PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_stt_whisper PRIVATE pthread m)
    endif()

    # ── test_stt_whisper_live: STT integration in impostazioni→voce ──
    add_executable(test_stt_whisper_live
        tests/test_stt_whisper_live.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_stt_whisper_live PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_stt_whisper_live PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_stt_whisper_live PRIVATE pthread m)
    endif()

    # ── test_onboarding: onboarding wizard QSettings, token LAN, rate limiter ──
    add_executable(test_onboarding
        tests/test_onboarding.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_onboarding PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_onboarding PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_onboarding PRIVATE pthread m)
    endif()

    add_executable(test_impostazioni_page
        tests/test_impostazioni_page.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_impostazioni_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_impostazioni_page PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_impostazioni_page PRIVATE pthread m)
    endif()

    add_executable(test_manutenzione_bugs
        tests/test_manutenzione_bugs.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_manutenzione_bugs PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_manutenzione_bugs PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_manutenzione_bugs PRIVATE pthread m)
    endif()

    add_executable(test_grafico
        tests/test_grafico.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_grafico PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_grafico PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_grafico PRIVATE pthread m)
    endif()

    add_executable(test_agenti_byzantine
        tests/test_agenti_byzantine.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_agenti_byzantine PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agenti_byzantine PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_agenti_byzantine PRIVATE pthread m)
    endif()

    # ── test_agente_autonomo: ReAct loop, toggle UI, parsing tool call ──
    add_executable(test_agente_autonomo
        tests/test_agente_autonomo.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_agente_autonomo PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_agente_autonomo PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_agente_autonomo PRIVATE pthread m)
    endif()

    add_executable(test_opencode_page
        tests/test_opencode_page.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_opencode_page PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_opencode_page PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_opencode_page PRIVATE pthread m)
    endif()

    add_executable(test_docker_sandbox
        tests/test_docker_sandbox.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_docker_sandbox PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_docker_sandbox PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_docker_sandbox PRIVATE pthread m)
    endif()

    add_executable(test_impara_quiz
        tests/test_impara_quiz.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_impara_quiz PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_impara_quiz PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_impara_quiz PRIVATE pthread m)
    endif()

    add_executable(test_monitor_panel
        tests/test_monitor_panel.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_monitor_panel PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_monitor_panel PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_monitor_panel PRIVATE pthread m)
    endif()

    # ── test_graph_memory: GraphMemory SQLite — nodi, archi, BFS, ricerca, export, prune ──
    add_executable(test_graph_memory
        tests/test_graph_memory.cpp
        graph_memory.cpp
    )
    target_include_directories(test_graph_memory PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_graph_memory PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_graph_memory PRIVATE pthread m)
    endif()

    # ── test_graph_memory_concurrent: accesso concorrente a GraphMemory (2 istanze stesso DB) ──
    add_executable(test_graph_memory_concurrent
        tests/test_graph_memory_concurrent.cpp
        graph_memory.cpp
    )
    target_include_directories(test_graph_memory_concurrent PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_graph_memory_concurrent PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_graph_memory_concurrent PRIVATE pthread m)
    endif()

    # ── test_pratico_finanza: normalizzaComune, cercaBelfiore, calcolaCodiceFiscale ──
    add_executable(test_pratico_finanza
        tests/test_pratico_finanza.cpp
    )
    target_include_directories(test_pratico_finanza PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_pratico_finanza PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_pratico_finanza PRIVATE pthread m)
    endif()

    # ── test_rag_graph_pipeline: pipeline RagGraph — file → LLM mock → parseAndStore → GraphMemory ──
    add_executable(test_rag_graph_pipeline
        tests/test_rag_graph_pipeline.cpp
        tests/mock_ai_client.h
        rag_graph.cpp
        graph_memory.cpp
    )
    target_include_directories(test_rag_graph_pipeline PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_rag_graph_pipeline PRIVATE test_ai_client_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_rag_graph_pipeline PRIVATE pthread m)
    endif()

    # ── test_astrale: CAT-A RicercaPage, CAT-B NatalChartWidget, CAT-C AstroCalc::compute ──
    add_executable(test_astrale
        tests/test_astrale.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_astrale PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_astrale PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_astrale PRIVATE pthread m)
    endif()

    # ── test_repl_python: CAT-A costruzione, CAT-B availability, CAT-C I/O, CAT-D robustezza ──
    add_executable(test_repl_python
        tests/test_repl_python.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_repl_python PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_repl_python PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_repl_python PRIVATE pthread m)
    endif()

    # ── test_file_parser: StrumentiFilePage costruzione, CSV, PDF, file non validi ──
    add_executable(test_file_parser
        tests/test_file_parser.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_file_parser PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_file_parser PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_file_parser PRIVATE pthread m)
    endif()

    # ── test_gns3_mcp: LanWanPage costruzione, GNS3 widget, combo azioni, host default ──
    add_executable(test_gns3_mcp
        tests/test_gns3_mcp.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_gns3_mcp PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_gns3_mcp PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_gns3_mcp PRIVATE pthread m)
    endif()

    # ── test_blhm_rab0l: Rab0lCanvas widget, BLHM Engine C, RicercaPage BLHM/RAB0-L ──
    add_executable(test_blhm_rab0l
        tests/test_blhm_rab0l.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_blhm_rab0l PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_blhm_rab0l PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_blhm_rab0l PRIVATE pthread m)
    endif()

    # ── test_distillazione: DistillazionePage — costruzione, stato iniziale (CAT-A) ──
    add_executable(test_distillazione
        tests/test_distillazione.cpp
        tests/mock_ai_client.h
    )
    target_include_directories(test_distillazione PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
    )
    target_link_libraries(test_distillazione PRIVATE test_shared_objs ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_distillazione PRIVATE pthread m)
    endif()

    # ── test_dep_check_panel: DepCheckPanel — costruzione, righe, runAllChecks()/segnali ──
    add_executable(test_dep_check_panel
        tests/test_dep_check_panel.cpp
        widgets/widget_dep_check.h
    )
    target_include_directories(test_dep_check_panel PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_dep_check_panel PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    if(NOT WIN32)
        target_link_libraries(test_dep_check_panel PRIVATE pthread m)
    endif()

    # ── test_perceptor_scripts: speaker_diarize.py (T-4) + fast_whisper_transcribe.py (T-5) ──
    add_executable(test_perceptor_scripts
        tests/test_perceptor_scripts.cpp
    )
    target_include_directories(test_perceptor_scripts PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_perceptor_scripts PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_perceptor_scripts PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_perceptor_scripts PRIVATE pthread m)
    endif()

    # ── test_streamlink_mcp: MCPs/streamlink_mcp/server.py — JSON-RPC 2.0, _validate_url SSRF (T-6) ──
    add_executable(test_streamlink_mcp
        tests/test_streamlink_mcp.cpp
    )
    target_include_directories(test_streamlink_mcp PRIVATE ${CMAKE_SOURCE_DIR})
    target_link_libraries(test_streamlink_mcp PRIVATE ${QT_LIBS} ${QT_TEST_LIB})
    target_compile_definitions(test_streamlink_mcp PRIVATE
        PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
    )
    if(NOT WIN32)
        target_link_libraries(test_streamlink_mcp PRIVATE pthread m)
    endif()

    enable_testing()
    add_test(NAME GraphMemory           COMMAND test_graph_memory)
    add_test(NAME GraphMemoryConcurrent COMMAND test_graph_memory_concurrent)
    add_test(NAME PraticoFinanza        COMMAND test_pratico_finanza)
    add_test(NAME SignalLifetime        COMMAND test_signal_lifetime)
    add_test(NAME RagEngine            COMMAND test_rag_engine)
    add_test(NAME CodeUtils            COMMAND test_code_utils)
    add_test(NAME RandomTool           COMMAND test_random_tool)
    add_test(NAME ExternalAiImport     COMMAND test_external_ai_import)
    add_test(NAME LavoroPage           COMMAND test_lavoro_page)
    add_test(NAME AppController        COMMAND test_app_controller)
    add_test(NAME McpManager           COMMAND test_mcp_manager)
    add_test(NAME ProgrammazionePage   COMMAND test_programmazione_page)
    add_test(NAME ThinkingDetect       COMMAND test_thinking_detect)
    add_test(NAME AiIntegration        COMMAND test_ai_integration)
    add_test(NAME AiStress             COMMAND test_ai_stress)
    add_test(NAME TeamCollab           COMMAND test_team_collab)
    add_test(NAME MultiAgenteLive      COMMAND test_multi_agente_live)
    add_test(NAME LavoroData           COMMAND test_lavoro_data)
    add_test(NAME TutorData            COMMAND test_tutor_data)
    add_test(NAME RagEngineAvanzato    COMMAND test_rag_engine_avanzato)
    add_test(NAME ChatHistory          COMMAND test_chat_history)
    add_test(NAME SimulatoreAlgos      COMMAND test_simulatore_algos)
    add_test(NAME FormulaParser        COMMAND test_formula_parser)
    add_test(NAME StrumentiRag         COMMAND test_strumenti_rag)
    add_test(NAME ThemeManager         COMMAND test_theme_manager)
    add_test(NAME ThemeManagerCrash    COMMAND test_theme_manager_crash)
    add_test(NAME HardwareMonitor      COMMAND test_hardware_monitor)
    add_test(NAME ManutenzioneeCron    COMMAND test_manutenzione_cron)
    add_test(NAME ChatHistoryStress    COMMAND test_chat_history_stress)
    add_test(NAME MatematicaPage       COMMAND test_matematica_page)
    add_test(NAME QrCodeWidget        COMMAND test_qr_code_widget)
    add_test(NAME HwDetectAmd         COMMAND test_hw_detect_amd)
    add_test(NAME KnowledgeInjection      COMMAND test_knowledge_injection)
    add_test(NAME AgentiPipeline          COMMAND test_agenti_pipeline)
    add_test(NAME AgentsConfigDialog      COMMAND test_agents_config_dialog)
    add_test(NAME AiClient                COMMAND test_ai_client)
    add_test(NAME LanWanCore              COMMAND test_lan_wan_core)
    add_test(NAME McpIntegration          COMMAND test_mcp_integration)
    add_test(NAME LanServer               COMMAND test_lan_server)
    add_test(NAME LanServerEndpoints      COMMAND test_lan_server_endpoints)
    add_test(NAME LanServerQueue          COMMAND test_lan_server_queue)
    add_test(NAME WanComputeTasks         COMMAND test_wan_compute_tasks)
    add_test(NAME SciCompute              COMMAND test_sci_compute)
    add_test(NAME Sintetizzatore          COMMAND test_sintetizzatore)
    add_test(NAME Onboarding              COMMAND test_onboarding)
    add_test(NAME ImpostazioniPage        COMMAND test_impostazioni_page)
    add_test(NAME ManutenzioneeBugs       COMMAND test_manutenzione_bugs)
    add_test(NAME Grafico                 COMMAND test_grafico)
    add_test(NAME AgentiByzantine         COMMAND test_agenti_byzantine)
    add_test(NAME AgenteAutonomo          COMMAND test_agente_autonomo)
    add_test(NAME OpenCodePage            COMMAND test_opencode_page)
    add_test(NAME DockerSandbox           COMMAND test_docker_sandbox)
    add_test(NAME ImparaQuiz              COMMAND test_impara_quiz)
    add_test(NAME MonitorPanel            COMMAND test_monitor_panel)
    add_test(NAME SttWhisper              COMMAND test_stt_whisper)
    add_test(NAME SttWhisperLive          COMMAND test_stt_whisper_live)
    add_test(NAME RagGraphPipeline        COMMAND test_rag_graph_pipeline)
    add_test(NAME Astrale                 COMMAND test_astrale)
    add_test(NAME BlhmRab0l               COMMAND test_blhm_rab0l)
    add_test(NAME Translitter             COMMAND test_translitter)
    add_test(NAME ReplPython              COMMAND test_repl_python)
    add_test(NAME FileParser              COMMAND test_file_parser)
    add_test(NAME Gns3Mcp                 COMMAND test_gns3_mcp)
    add_test(NAME Distillazione           COMMAND test_distillazione)
    add_test(NAME DepCheckPanel           COMMAND test_dep_check_panel)
    add_test(NAME PerceptorScripts        COMMAND test_perceptor_scripts)
    add_test(NAME StreamlinkMcp           COMMAND test_streamlink_mcp)
    set_tests_properties(RagGraphPipeline PROPERTIES TIMEOUT 30)
    set_tests_properties(ReplPython       PROPERTIES TIMEOUT 30)
    set_tests_properties(AiIntegration  PROPERTIES TIMEOUT 900)   # 15 min — CPU-only mistral lento
    set_tests_properties(AiStress       PROPERTIES TIMEOUT 1800)  # 30 min — 6 test reali + stress
    set_tests_properties(TeamCollab PROPERTIES TIMEOUT 2400)  # 40 min totali
    set_tests_properties(RagEngineAvanzato  PROPERTIES TIMEOUT 120)
    set_tests_properties(SimulatoreAlgos    PROPERTIES TIMEOUT 120 RESOURCE_LOCK cpu_heavy)
    set_tests_properties(HardwareMonitor    PROPERTIES TIMEOUT 90)   # initTestCase + 2x thread hw_detect
    set_tests_properties(ChatHistoryStress  PROPERTIES TIMEOUT 60)
    set_tests_properties(MultiAgenteLive PROPERTIES TIMEOUT 300)  # 5 min — pipeline completa
    # Serializza i test che usano Ollama reale: con -j4 girano in parallelo e saturano RAM/CPU
    set_tests_properties(AiIntegration AiStress TeamCollab MultiAgenteLive
        PROPERTIES RESOURCE_LOCK ollama)
    set_tests_properties(GraphMemory            PROPERTIES TIMEOUT 30)
    set_tests_properties(GraphMemoryConcurrent  PROPERTIES TIMEOUT 30)
    set_tests_properties(LanServer              PROPERTIES TIMEOUT 30)
    set_tests_properties(LanServerEndpoints     PROPERTIES TIMEOUT 30)
    set_tests_properties(ThemeManagerCrash      PROPERTIES TIMEOUT 30)
    set_tests_properties(WanComputeTasks    PROPERTIES TIMEOUT 60)
    set_tests_properties(SciCompute         PROPERTIES TIMEOUT 30)
    set_tests_properties(Sintetizzatore     PROPERTIES TIMEOUT 30)
    set_tests_properties(ImpostazioniPage   PROPERTIES TIMEOUT 60)
    set_tests_properties(SignalLifetime     PROPERTIES TIMEOUT 60 RESOURCE_LOCK cpu_heavy)
    set_tests_properties(LavoroPage         PROPERTIES TIMEOUT 120 RESOURCE_LOCK cpu_heavy)
    set_tests_properties(ManutenzioneeBugs  PROPERTIES TIMEOUT 30)
    set_tests_properties(Grafico            PROPERTIES TIMEOUT 30)
    set_tests_properties(AgentiByzantine    PROPERTIES TIMEOUT 30)
    set_tests_properties(OpenCodePage       PROPERTIES TIMEOUT 30)
    set_tests_properties(FileParser         PROPERTIES TIMEOUT 30)
    set_tests_properties(ImparaQuiz         PROPERTIES TIMEOUT 30)
    set_tests_properties(MonitorPanel       PROPERTIES TIMEOUT 30)
    message(STATUS "Build test abilitata: graph_memory, signal_lifetime, rag_engine, code_utils, random_tool, lavoro_page, app_controller, programmazione_page, thinking_detect, ai_integration, ai_stress, team_collab, lavoro_data, tutor_data, rag_engine_avanzato, chat_history, simulatore_algos, formula_parser, strumenti_rag, theme_manager, theme_manager_crash, hardware_monitor, manutenzione_cron, chat_history_stress, matematica_page, qr_code_widget, hw_detect_amd, knowledge_injection, lan_server_endpoints")

    # Applica ASAN+UBSAN anche a tutti i test target quando ENABLE_SANITIZERS=ON
    if(ENABLE_SANITIZERS AND NOT MSVC)
        get_property(_all_tests DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(_t IN LISTS _all_tests)
            if(_t MATCHES "^test_")
                target_compile_options(${_t} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
                target_link_options   (${_t} PRIVATE -fsanitize=address,undefined)
            endif()
        endforeach()
        message(STATUS "Test: ASAN+UBSAN abilitati su tutti i target test_*")
    endif()
endif()

# ── Documentazione ASAN ───────────────────────────────────────────────────
# Abilitare i sanitizer:
#   cmake -B build_asan -DENABLE_SANITIZERS=ON -DBUILD_TESTS=ON \
#         -DCMAKE_BUILD_TYPE=RelWithDebInfo
#   cmake --build build_asan -j$(nproc)
#   ASAN_OPTIONS=halt_on_error=1:detect_leaks=1 ctest --test-dir build_asan -j1

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
