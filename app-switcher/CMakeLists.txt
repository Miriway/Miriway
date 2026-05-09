set(PROTOCOLS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wayland-protocols)
set(GENERATED_DIR ${CMAKE_CURRENT_BINARY_DIR}/wayland-generated)

function(add_wayland_client_protocol xml_file out_var)
    get_filename_component(name ${xml_file} NAME_WE)
    set(header ${GENERATED_DIR}/${name}.h)
    set(code   ${GENERATED_DIR}/${name}.c)

    add_custom_command(
        OUTPUT ${header}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
        COMMAND wayland-scanner client-header "${xml_file}" "${header}.tmp"
        COMMAND sed -i "s/\\<namespace\\>/namespace_/g" "${header}.tmp"
        COMMAND ${CMAKE_COMMAND} -E rename ${header}.tmp ${header}
        DEPENDS ${xml_file}
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${code}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_DIR}
        COMMAND wayland-scanner private-code "${xml_file}" "${code}.tmp"
        COMMAND sed -i "s/\\<namespace\\>/namespace_/g" "${code}.tmp"
        COMMAND ${CMAKE_COMMAND} -E rename ${code}.tmp ${code}
        DEPENDS ${xml_file}
        VERBATIM
    )

    list(APPEND ${out_var} ${header} ${code})
    set(${out_var} ${${out_var}} PARENT_SCOPE)
endfunction()

add_wayland_client_protocol(
    ${PROTOCOLS_DIR}/xdg-shell.xml
    PROTOCOL_SOURCES)

add_wayland_client_protocol(
    ${PROTOCOLS_DIR}/wlr-layer-shell-unstable-v1.xml
    PROTOCOL_SOURCES)

add_wayland_client_protocol(
    ${PROTOCOLS_DIR}/wlr-foreign-toplevel-management-unstable-v1.xml
    PROTOCOL_SOURCES)

pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(MIRCOMMON REQUIRED IMPORTED_TARGET mircommon)
pkg_check_modules(WAYLANDCLIENT REQUIRED IMPORTED_TARGET wayland-client)

add_library(app-switcher STATIC
    application_switcher.h
    application_switcher.cpp
    wayland_app.h
    wayland_app.cpp
    wayland_shm.h
    wayland_shm.cpp
    layer_shell_wayland_surface.h
    layer_shell_wayland_surface.cpp
    ${PROTOCOL_SOURCES}
)

target_include_directories(app-switcher
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${GENERATED_DIR}
)

target_link_libraries(app-switcher
    PUBLIC
        PkgConfig::MIRAL
    PRIVATE
        PkgConfig::FREETYPE
        PkgConfig::MIRCOMMON
        PkgConfig::WAYLANDCLIENT
)

