cmake_minimum_required(VERSION 3.16)
project(Prismalux_GUI VERSION 2.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# ── Qt6 con fallback a Qt5 ────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Widgets Network Sql QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql)
    message(STATUS "Prismalux GUI: usando Qt6")
else()
    find_package(Qt5 5.15 COMPONENTS Core Widgets Network Sql REQUIRED)
    set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network Qt5::Sql)
    message(STATUS "Prismalux GUI: usando Qt5")
endif()

# ── Backend C: solo hw_detect (rilevamento GPU cross-platform) ───────────
set(C_SRCS
    ../C_software/src/hw_detect.c
)
set_source_files_properties(${C_SRCS} PROPERTIES LANGUAGE C)

# ── Sorgenti C++ Qt ──────────────────────────────────────────────────────
set(CPP_SRCS
    main.cpp
    mainwindow.cpp
    hardware_monitor.cpp
    ai_client.cpp
    theme_manager.cpp
    pages/agenti_page.cpp
    pages/pratico_page.cpp
    pages/impara_page.cpp
    pages/materie_page.cpp
    pages/tutor_data.cpp
    pages/personalizza_page.cpp
    pages/manutenzione_page.cpp
    pages/impostazioni_page.cpp
    pages/simulatore_page.cpp
    pages/programmazione_page.cpp
    monitor_panel.cpp
    chat_history.cpp
    # Widget header-only con Q_OBJECT: vanno elencati per AUTOMOC
    widgets/spinner_widget.h
    widgets/status_badge.h
)

set(QRC_FILES
    themes/themes.qrc
)

add_executable(Prismalux_GUI ${CPP_SRCS} ${C_SRCS} ${QRC_FILES})

target_include_directories(Prismalux_GUI PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/../C_software/include
)

target_link_libraries(Prismalux_GUI PRIVATE ${QT_LIBS})

# Percorso assoluto della root Prismalux (usato da personalizza_page per gcc command)
target_compile_definitions(Prismalux_GUI PRIVATE
    PRISMALUX_ROOT="${CMAKE_SOURCE_DIR}/.."
)

if(WIN32)
    target_link_libraries(Prismalux_GUI PRIVATE ws2_32)
    target_compile_definitions(Prismalux_GUI PRIVATE
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    # Nasconde la console su Windows (app grafica pura)
    set_target_properties(Prismalux_GUI PROPERTIES WIN32_EXECUTABLE TRUE)
    # Hint post-build: copia DLL Qt (esegui manualmente se non in PATH)
    find_program(WINDEPLOYQT windeployqt HINTS "${Qt6_DIR}/../../../bin")
    if(WINDEPLOYQT)
        add_custom_command(TARGET Prismalux_GUI POST_BUILD
            COMMAND ${WINDEPLOYQT} $<TARGET_FILE:Prismalux_GUI>
            COMMENT "Copia DLL Qt con windeployqt...")
    endif()
else()
    target_link_libraries(Prismalux_GUI PRIVATE pthread m)
endif()

# Copia il foglio stile nella directory di build
configure_file(style.qss ${CMAKE_BINARY_DIR}/style.qss COPYONLY)
