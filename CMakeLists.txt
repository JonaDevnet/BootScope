cmake_minimum_required(VERSION 3.16)
project(BootScope VERSION 0.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(BS_SOURCES
    src/main.c
    src/app.c
    src/ui.c
    src/nk_impl.c
)

add_executable(bootscope WIN32 ${BS_SOURCES})

target_include_directories(bootscope PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_compile_definitions(bootscope PRIVATE
    BS_VERSION="${PROJECT_VERSION}"
    UNICODE_DISABLED
)

target_link_libraries(bootscope PRIVATE
    user32
    gdi32
    msimg32
    shell32
    advapi32
)

if(MSVC)
    target_compile_options(bootscope PRIVATE /utf-8 /W3 /O2)
    target_compile_definitions(bootscope PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(bootscope PRIVATE -Wall -Wextra -O2)
endif()

install(TARGETS bootscope RUNTIME DESTINATION .)
