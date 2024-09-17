cmake_minimum_required(VERSION 3.5)
cmake_policy(SET CMP0015 NEW)
cmake_policy(SET CMP0022 NEW)

project(miriway)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Werror -Wall -pedantic -Wextra -fPIC -Wnon-virtual-dtor")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,defs")

include(GNUInstallDirs)
include(FindPkgConfig)
include(CheckIncludeFileCXX)

pkg_check_modules(MIRAL miral>=5.1 REQUIRED)
pkg_check_modules(XKBCOMMON xkbcommon REQUIRED)

add_executable(miriway-shell
    miriway.cpp
    miriway_commands.cpp miriway_commands.h
    miriway_policy.cpp   miriway_policy.h
    miriway_workspace_manager.cpp miriway_workspace_manager.h
)

target_include_directories(miriway-shell PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS})
target_link_libraries(     miriway-shell               ${MIRAL_LDFLAGS})
target_link_libraries(     miriway-shell               ${XKBCOMMON_LIBRARIES})

set (CMAKE_REQUIRED_INCLUDES "${MIRAL_INCLUDE_DIRS}")
include_directories(miriway-shell PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS})
target_compile_definitions(miriway-shell PRIVATE MIR_LOG_COMPONENT="miriway")

add_custom_target(miriway ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-terminal ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-terminal ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-unsnap ALL
        cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-unsnap ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-unconfine ALL
        cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-unconfine ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-background ALL
        cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-background ${CMAKE_BINARY_DIR}
)

install(PROGRAMS
    ${CMAKE_BINARY_DIR}/miriway
    ${CMAKE_BINARY_DIR}/miriway-shell
    ${CMAKE_BINARY_DIR}/miriway-terminal
    ${CMAKE_BINARY_DIR}/miriway-unsnap
    ${CMAKE_BINARY_DIR}/miriway-unconfine
    ${CMAKE_BINARY_DIR}/miriway-background
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${CMAKE_SOURCE_DIR}/miriway.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions/
)

install(FILES ${CMAKE_SOURCE_DIR}/miriway-shell.config
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/xdg-miriway
)

