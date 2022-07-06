cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0022 NEW)

project(miriway)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC -Wnon-virtual-dtor")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs")

include(FindPkgConfig)
pkg_check_modules(MIRAL miral REQUIRED)
pkg_check_modules(MIRCOMMON mircommon REQUIRED)
pkg_check_modules(FREETYPE freetype2 REQUIRED)
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(XKBCOMMON xkbcommon REQUIRED)
find_package(Boost COMPONENTS filesystem REQUIRED)

add_executable(miriway-shell
    miriway.cpp
    miriway_commands.cpp miriway_commands.h
    miriway_policy.cpp   miriway_policy.h
)

set_source_files_properties(miriway.cpp PROPERTIES COMPILE_DEFINITIONS EGMDE_WALLPAPER_BOTTOM="${EGMDE_WALLPAPER_BOTTOM}")
target_include_directories(miriway-shell PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS} ${MIRCOMMON_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(     miriway-shell               ${MIRAL_LDFLAGS}      ${MIRCOMMON_LDFLAGS}      ${WAYLAND_CLIENT_LIBRARIES}  ${Boost_LIBRARIES}    ${FREETYPE_LIBRARIES})
target_link_libraries(     miriway-shell               ${XKBCOMMON_LIBRARIES})
set_target_properties(     miriway-shell PROPERTIES COMPILE_DEFINITIONS MIR_LOG_COMPONENT="miriway")

add_custom_target(miriway ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-terminal ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-terminal ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-background ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-background ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-panel ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-panel ${CMAKE_BINARY_DIR}
)

install(PROGRAMS
    ${CMAKE_BINARY_DIR}/miriway
    ${CMAKE_BINARY_DIR}/miriway-shell
    ${CMAKE_BINARY_DIR}/miriway-background
    ${CMAKE_BINARY_DIR}/miriway-terminal
    ${CMAKE_BINARY_DIR}/miriway-panel
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)

install(FILES ${CMAKE_SOURCE_DIR}/miriway.desktop
    DESTINATION /usr/share/wayland-sessions/
)

