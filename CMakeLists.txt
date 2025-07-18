cmake_minimum_required(VERSION 3.10)
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

option(SDDM "Install configuration for SDDM?" OFF)

pkg_check_modules(MIRAL miral>=5.1 REQUIRED)
pkg_check_modules(MIRWAYLAND mirwayland REQUIRED)
pkg_check_modules(XKBCOMMON xkbcommon REQUIRED)

set (CMAKE_REQUIRED_INCLUDES "${MIRAL_INCLUDE_DIRS}")

add_subdirectory(wayland-generated)

add_library(miriwaycommon STATIC
    miriway_child_control.cpp       miriway_child_control.h
    miriway_commands.cpp            miriway_commands.h
    miriway_workspace_manager.cpp   miriway_workspace_manager.h miriway_workspace_hooks.h
    wayland-generated/ext-workspace-v1_wrapper.cpp    wayland-generated/ext-workspace-v1_wrapper.h
    miriway_ext_workspace_v1.cpp    miriway_ext_workspace_v1.h
)

target_include_directories(miriwaycommon PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS} ${MIRWAYLAND_INCLUDE_DIRS})
target_link_libraries(     miriwaycommon PUBLIC        ${MIRAL_LDFLAGS} ${MIRWAYLAND_LDFLAGS} ${XKBCOMMON_LIBRARIES})
target_compile_definitions(miriwaycommon PRIVATE MIR_LOG_COMPONENT="miriway")

add_executable(miriway-shell
    miriway.cpp
    miriway_policy.cpp              miriway_policy.h
)
target_link_libraries(miriway-shell miriwaycommon)
target_compile_definitions(miriway-shell PRIVATE MIR_LOG_COMPONENT="miriway")

add_executable(miriway-run-shell
    miriway-run-shell.cpp
)
target_link_libraries(miriway-run-shell ${MIRAL_LDFLAGS})
target_include_directories(miriway-run-shell PUBLIC SYSTEM ${MIRAL_INCLUDE_DIRS})
target_compile_definitions(miriway-run-shell PRIVATE MIR_LOG_COMPONENT="miriway")

add_custom_target(miriway ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-run ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-run ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-terminal ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-terminal ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-unsnap ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-unsnap ${CMAKE_BINARY_DIR}
)

add_custom_target(miriway-background ALL
    cp ${CMAKE_CURRENT_SOURCE_DIR}/miriway-background ${CMAKE_BINARY_DIR}
)

install(PROGRAMS
    ${CMAKE_BINARY_DIR}/miriway
    ${CMAKE_BINARY_DIR}/miriway-run
    ${CMAKE_BINARY_DIR}/miriway-run-shell
    ${CMAKE_BINARY_DIR}/miriway-shell
    ${CMAKE_BINARY_DIR}/miriway-session
    ${CMAKE_BINARY_DIR}/miriway-terminal
    ${CMAKE_BINARY_DIR}/miriway-unsnap
    ${CMAKE_BINARY_DIR}/miriway-background
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/miriway-session.in miriway-session @ONLY)

install(FILES ${CMAKE_SOURCE_DIR}/miriway.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/wayland-sessions/
)

install(FILES ${CMAKE_SOURCE_DIR}/miriway-shell.config
    DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/xdg/xdg-miriway
)

install(PROGRAMS ${CMAKE_SOURCE_DIR}/systemd/usr/libexec/miriway-session-startup
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

install(PROGRAMS ${CMAKE_SOURCE_DIR}/systemd/usr/libexec/miriway-session-shutdown
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}
)

install(FILES ${CMAKE_SOURCE_DIR}/systemd/usr/lib/systemd/user/miriway-session.target
    DESTINATION /usr/lib/systemd/user/
)
if(SDDM)
    configure_file(${CMAKE_SOURCE_DIR}/sddm/miriway.conf.in
        ${CMAKE_BINARY_DIR}/sddm/miriway.conf @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/sddm/miriway.conf
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/sddm.conf.d/
    )
endif()
