cmake_minimum_required(VERSION 3.16)
project(BootScope VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BS_SOURCES
    src/main.c
    src/app.c
    src/ui.c
    src/nk_impl.c
    src/sys/util.c
    src/sys/osinfo.c
    src/sys/boot.c
    src/sys/scm.c
    src/sys/startup.c
    src/sys/net.c
    src/sys/remote.c
    src/sys/hw.c
    src/core/scan.c
    src/core/checks.c
    src/core/score.c
    src/core/report.c
    src/core/actions.c
)

add_executable(bootscope WIN32 ${BS_SOURCES})

target_include_directories(bootscope PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_compile_definitions(bootscope PRIVATE
    BS_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(bootscope PRIVATE
    user32
    gdi32
    msimg32
    shell32
    advapi32
    wevtapi
    iphlpapi
    ws2_32
    ole32
)

if(MSVC)
    target_compile_options(bootscope PRIVATE /utf-8 /W3 /O2)
    target_compile_definitions(bootscope PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(bootscope PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS bootscope RUNTIME DESTINATION .)

option(BS_BUILD_TESTS "Compilar las pruebas del motor de acciones" OFF)
if(BS_BUILD_TESTS)
    add_executable(test_actions
        tests/test_actions.c
        src/core/actions.c
        src/sys/util.c
        src/app.c
    )
    target_include_directories(test_actions PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    target_link_libraries(test_actions PRIVATE user32 advapi32)
endif()


