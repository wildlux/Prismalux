cmake_minimum_required(VERSION 3.16)
project(Prismalux_GUI VERSION 2.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
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
    ../src/hw_detect.c
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
    pages/agenti_page.cpp
    pages/agents_config_dialog.cpp
    pages/pratico_page.cpp
    pages/impara_page.cpp
    pages/quiz_page.cpp
    pages/oracolo_page.cpp
    pages/materie_page.cpp
    pages/tutor_data.cpp
    pages/personalizza_page.cpp
    pages/manutenzione_page.cpp
    pages/impostazioni_page.cpp
    pages/simulatore_page.cpp
    pages/strumenti_page.cpp
    pages/grafico_page.cpp
    pages/programmazione_page.cpp
    pages/matematica_page.cpp
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
    widgets/whisper_autosetup.h
)

set(QRC_FILES
    themes/themes.qrc
)

add_executable(Prismalux_GUI ${CPP_SRCS} ${C_SRCS} ${QRC_FILES})

target_include_directories(Prismalux_GUI PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../include
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

# Copia il foglio stile nella directory di build
configure_file(style.qss ${CMAKE_BINARY_DIR}/style.qss COPYONLY)

message(STATUS "Prismalux GUI: grafico_page include SmithPrime (idx 7) + MathConst-pi-e-Primi (idx 8)")
