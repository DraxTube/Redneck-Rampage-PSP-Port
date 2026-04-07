cmake_minimum_required(VERSION 3.11)

project(RedneckRampagePSP C)

# PSP application metadata
set(PSP_APP_NAME "Redneck Rampage")
set(PSP_APP_VER "1.0.0")

# Source files - Engine core
set(ENGINE_SOURCES
    src/engine/engine.c
    src/engine/cache.c
)

# Source files - File parsers
set(UTIL_SOURCES
    src/util/grp.c
    src/util/art.c
    src/util/map.c
)

# Source files - PSP platform layer
set(PLATFORM_SOURCES
    src/platform/psp_main.c
    src/platform/psp_display.c
    src/platform/psp_input.c
    src/platform/psp_audio.c
    src/platform/psp_timer.c
)

# Source files - Game logic
set(GAME_SOURCES
    src/game/game.c
    src/game/player.c
    src/game/actors.c
    src/game/weapons.c
    src/game/sectors.c
    src/game/menus.c
)

# All sources
set(ALL_SOURCES
    ${ENGINE_SOURCES}
    ${UTIL_SOURCES}
    ${PLATFORM_SOURCES}
    ${GAME_SOURCES}
)

# Create executable
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/util
)

# Compiler flags optimized for PSP MIPS
target_compile_options(${PROJECT_NAME} PRIVATE
    -O2
    -G0
    -Wall
    -Wno-unused-variable
    -Wno-unused-function
    -ffast-math
    -fsingle-precision-constant
)

# Definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PSP
    __PSP__
)

# Link PSP libraries (ORDER MATTERS!)
target_link_libraries(${PROJECT_NAME}
    pspdebug
    pspdisplay
    pspge
    pspctrl
    pspaudio
    psppower
    psprtc
    psputility
    pspsdk
    pspuser
    pspkernel
    m
    c
)

# Create EBOOT.PBP
create_pbp_file(
    TARGET ${PROJECT_NAME}
    ICON_PATH NULL
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE "${PSP_APP_NAME}"
)
