#=============================================================================
# sm_tracer TUI desktop build
# Keep the root CMakeLists lean and delegate compile settings to the
# toolchain and presets.
#=============================================================================

#=============================================================================
# ===Required minimal cmake version.
cmake_minimum_required(VERSION 3.25)

#=============================================================================
# ===Name of project.
project(sm_tracer_tui LANGUAGES C)

#=============================================================================
# ===Output directories.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

#=============================================================================
# ===Component local.
set(ROOT_LOCAL ${CMAKE_SOURCE_DIR})
set(INC_LOCAL
    ${ROOT_LOCAL}/application
    ${ROOT_LOCAL}/application/ui
    ${ROOT_LOCAL}/application/aos
)
file(GLOB_RECURSE SRC_LOCAL
    ${ROOT_LOCAL}/application/*.c
)

#=============================================================================
# ===Component notcurses.
find_path(NOTCURSES_INCLUDE_DIR notcurses/notcurses.h)
find_library(NOTCURSES_LIBRARY notcurses-core)

if(NOT NOTCURSES_INCLUDE_DIR OR NOT NOTCURSES_LIBRARY)
    message(FATAL_ERROR "notcurses-core not found")
endif()

#=============================================================================
# ===Component libserialport.
set(SERIALPORT_ROOT ${CMAKE_SOURCE_DIR}/3rd_party/libserialport)
add_library(serialport SHARED IMPORTED GLOBAL)

if(WIN32)
    set(SERIALPORT_INCLUDE_DIR "${SERIALPORT_ROOT}/windows/include")
    set_target_properties(serialport PROPERTIES
        IMPORTED_LOCATION
            "${SERIALPORT_ROOT}/windows/libserialport-0.dll"
        IMPORTED_IMPLIB
            "${SERIALPORT_ROOT}/windows/libserialport.dll.a"
        INTERFACE_INCLUDE_DIRECTORIES
            "${SERIALPORT_INCLUDE_DIR}"
    )
else()
    set(SERIALPORT_INCLUDE_DIR "${SERIALPORT_ROOT}/linux/include")
    set_target_properties(serialport PROPERTIES
        IMPORTED_LOCATION
            "${SERIALPORT_ROOT}/linux/libserialport.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${SERIALPORT_INCLUDE_DIR}"
    )
endif()

#=============================================================================
# ===Component SST core.
set(SST_ROOT   ${CMAKE_SOURCE_DIR}/3rd_party/sm_sst)
set(SST_SRC
    ${SST_ROOT}/sst_c/src/sst.c
    ${SST_ROOT}/sst_c/src/sst_pubsub.c
)
set(SST_INC    ${SST_ROOT}/include)

#=============================================================================
# ===Component ports.
set(PORT_SRC
    ${CMAKE_SOURCE_DIR}/ports/sst/sst_port.c
    ${CMAKE_SOURCE_DIR}/ports/sst/sst_evt_pool.c
)
set(PORT_INC
    ${CMAKE_SOURCE_DIR}/ports/sst
    ${CMAKE_SOURCE_DIR}/ports/sm
)

#=============================================================================
# ===Component sm_hsm.
set(SM_HSM_ROOT ${CMAKE_SOURCE_DIR}/3rd_party/sm_hsm)
set(SM_HSM_SRC ${SM_HSM_ROOT}/src/sm_hsm.c)
set(SM_HSM_INC ${SM_HSM_ROOT}/include)

#=============================================================================
# ===Component static_pool (from common_c).
set(COMMON_C_INC ${CMAKE_SOURCE_DIR}/3rd_party/common_c/include)
set(STATIC_POOL_SRC ${CMAKE_SOURCE_DIR}/3rd_party/common_c/src/static_pool/static_pool.c)

#=============================================================================
# ===Component BSP.
set(BSP_SRC ${CMAKE_SOURCE_DIR}/bsp/bsp.c)
set(BSP_INC ${CMAKE_SOURCE_DIR}/bsp)

#=============================================================================
# ===Collecting all sources.
set(SRC_ALL
    ${SRC_LOCAL}
    ${SST_SRC}
    ${PORT_SRC}
    ${SM_HSM_SRC}
    ${STATIC_POOL_SRC}
    ${BSP_SRC}
)

set(INC_ALL
    ${INC_LOCAL}
    ${BSP_INC}
    ${NOTCURSES_INCLUDE_DIR}
    ${SST_INC}
    ${PORT_INC}
    ${SM_HSM_INC}
    ${COMMON_C_INC}
)

#=============================================================================
# ===Target rules.
add_executable(${PROJECT_NAME}
    ${SRC_ALL}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${INC_ALL}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    _DEFAULT_SOURCE
    _POSIX_C_SOURCE=200809L
    _XOPEN_SOURCE=700
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${NOTCURSES_LIBRARY}
    serialport
)

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SERIALPORT_ROOT}/windows/libserialport-0.dll"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
else()
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${SERIALPORT_ROOT}/linux"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()

#=============================================================================
# ===Tests and compile-time interface contracts.
include(CTest)
if(BUILD_TESTING)
    add_library(ui_event_ingress_contract OBJECT
        tests/ui_event_ingress_contract.c
    )
    target_include_directories(ui_event_ingress_contract PRIVATE
        ${ROOT_LOCAL}/application/ui
    )

    add_library(application_subsystem_contract OBJECT
        tests/application_subsystem_contract.c
    )
    target_include_directories(application_subsystem_contract PRIVATE
        ${INC_ALL}
    )

    add_library(ui_input_composer_contract OBJECT
        tests/ui_input_composer_contract.c
    )
    target_include_directories(ui_input_composer_contract PRIVATE
        ${INC_ALL}
    )

    add_executable(ui_input_router_test
        tests/ui_input_router_test.c
        application/ui/thread/ui_input_router.c
    )
    target_include_directories(ui_input_router_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(ui_input_router_test PRIVATE
        DBC_DISABLE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
    target_link_libraries(ui_input_router_test PRIVATE
        ${NOTCURSES_LIBRARY}
    )
    add_test(NAME ui_input_router
             COMMAND ui_input_router_test)

    add_executable(ui_input_cmps_mngr_test
        tests/ui_input_cmps_mngr_test.c
        application/ui/hsm/sm_input_cmps_mngr.c
        ${SM_HSM_SRC}
    )
    target_include_directories(ui_input_cmps_mngr_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(ui_input_cmps_mngr_test PRIVATE
        SM_DBC_DISABLE
    )
    add_test(NAME ui_input_cmps_mngr
             COMMAND ui_input_cmps_mngr_test)

    add_executable(sp_mngr_command_test
        tests/sp_mngr_command_test.c
        application/aos/sp_mngr/sp_mngr.c
        application/aos/sp_mngr/hdlc_parser.c
        ${SM_HSM_SRC}
    )
    target_include_directories(sp_mngr_command_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(sp_mngr_command_test PRIVATE
        DBC_DISABLE
        SM_DBC_DISABLE
    )
    add_test(NAME sp_mngr_command
             COMMAND sp_mngr_command_test)

    add_executable(hdlc_parser_test
        tests/hdlc_parser_test.c
        application/aos/sp_mngr/hdlc_parser.c
        ${SM_HSM_SRC}
    )
    target_include_directories(hdlc_parser_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(hdlc_parser_test PRIVATE
        DBC_DISABLE
        SM_DBC_DISABLE
    )
    add_test(NAME hdlc_parser
             COMMAND hdlc_parser_test)

    add_executable(sp_thread_event_flow_test
        tests/sp_thread_event_flow_test.c
        application/sp_thread/thread/sp_thread.c
        application/sp_thread/thread/sp_thread_evt.c
        application/sp_thread/thread/sp_thread_wake.c
        application/sp_thread/thread/rx_packet_assembler.c
        application/sp_thread/hsm/sm_sp_thread.c
        ${SM_HSM_SRC}
    )
    target_include_directories(sp_thread_event_flow_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(sp_thread_event_flow_test PRIVATE
        DBC_DISABLE
        SM_DBC_DISABLE
        _POSIX_C_SOURCE=200809L
    )
    add_test(NAME sp_thread_event_flow
             COMMAND sp_thread_event_flow_test)

    add_executable(sp_thread_event_contract_test
        tests/sp_thread_event_contract_test.c
        application/sp_thread/thread/sp_thread_evt.c
    )
    target_include_directories(sp_thread_event_contract_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(sp_thread_event_contract_test PRIVATE
        DBC_DISABLE
        _POSIX_C_SOURCE=200809L
    )
    add_test(NAME sp_thread_event_contract
             COMMAND sp_thread_event_contract_test)

    add_executable(sp_thread_wake_test
        tests/sp_thread_wake_test.c
        application/sp_thread/thread/sp_thread_wake.c
    )
    target_include_directories(sp_thread_wake_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(sp_thread_wake_test PRIVATE
        DBC_DISABLE
    )
    add_test(NAME sp_thread_wake
             COMMAND sp_thread_wake_test)

    add_executable(serial_port_runtime_test
        tests/serial_port_runtime_test.c
        application/sp_thread/thread/serial_port_runtime.c
    )
    target_include_directories(serial_port_runtime_test PRIVATE
        ${INC_ALL}
        ${SERIALPORT_INCLUDE_DIR}
    )
    add_test(NAME serial_port_runtime
             COMMAND serial_port_runtime_test)

    add_executable(rx_packet_assembler_test
        tests/rx_packet_assembler_test.c
        application/sp_thread/thread/rx_packet_assembler.c
    )
    target_include_directories(rx_packet_assembler_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(rx_packet_assembler_test PRIVATE
        DBC_DISABLE
    )
    add_test(NAME rx_packet_assembler
             COMMAND rx_packet_assembler_test)

    add_executable(ui_port_list_event_test
        tests/ui_port_list_event_test.c
        application/ui/thread/ui_evt.c
    )
    target_include_directories(ui_port_list_event_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(ui_port_list_event_test PRIVATE
        DBC_DISABLE
        _POSIX_C_SOURCE=200809L
    )
    add_test(NAME ui_port_list_event
             COMMAND ui_port_list_event_test)

    add_executable(ui_input_composer_lifecycle_test
        tests/ui_input_composer_lifecycle.c
        application/ui/widgets/input_composer.c
        application/ui/widgets/widget_io_priv.c
    )
    target_include_directories(ui_input_composer_lifecycle_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(ui_input_composer_lifecycle_test PRIVATE
        DBC_DISABLE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
    target_link_libraries(ui_input_composer_lifecycle_test PRIVATE
        ${NOTCURSES_LIBRARY}
    )
    add_test(NAME ui_input_composer_lifecycle
             COMMAND ui_input_composer_lifecycle_test)
    set_tests_properties(ui_input_composer_lifecycle PROPERTIES
        ENVIRONMENT "TERM=xterm-256color"
    )

    add_executable(ui_command_suggestion_lifecycle_test
        tests/ui_command_suggestion_lifecycle.c
        application/ui/widgets/command_suggestion.c
    )
    target_include_directories(ui_command_suggestion_lifecycle_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(ui_command_suggestion_lifecycle_test PRIVATE
        DBC_DISABLE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
    target_link_libraries(ui_command_suggestion_lifecycle_test PRIVATE
        ${NOTCURSES_LIBRARY}
    )
    add_test(NAME ui_command_suggestion_lifecycle
             COMMAND ui_command_suggestion_lifecycle_test)
    set_tests_properties(ui_command_suggestion_lifecycle PROPERTIES
        ENVIRONMENT "TERM=xterm-256color"
    )

    add_executable(scrollbar_test
        tests/scrollbar_test.c
        application/ui/widgets/text_buffer_view.c
    )
    target_include_directories(scrollbar_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(scrollbar_test PRIVATE
        DBC_DISABLE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
    target_compile_options(scrollbar_test PRIVATE
        -UNDEBUG
    )
    target_link_libraries(scrollbar_test PRIVATE
        ${NOTCURSES_LIBRARY}
    )
    add_test(NAME scrollbar
             COMMAND scrollbar_test)

    add_executable(text_area_test
        tests/text_area_test.c
        application/ui/widgets/text_buffer_view.c
    )
    target_include_directories(text_area_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(text_area_test PRIVATE
        DBC_DISABLE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
    target_compile_options(text_area_test PRIVATE
        -UNDEBUG
    )
    target_link_libraries(text_area_test PRIVATE
        ${NOTCURSES_LIBRARY}
    )
    add_test(NAME text_area
             COMMAND text_area_test)

    add_test(NAME ui_widget_boundary_contract
             COMMAND ${CMAKE_SOURCE_DIR}/tests/ui_widget_boundary_contract.sh)
endif()

#=============================================================================
# ===Run helper target.
add_custom_target(run
    COMMAND $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    USES_TERMINAL
    VERBATIM
)
