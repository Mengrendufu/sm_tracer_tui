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

include(CTest)

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
set(NOTCURSES_ROOT ${CMAKE_SOURCE_DIR}/3rd_party/notcurses)
set(NOTCURSES_BINARY_DIR ${CMAKE_BINARY_DIR}/third_party/notcurses)

set(USE_CXX OFF CACHE BOOL "" FORCE)
set(USE_DOCTEST OFF CACHE BOOL "" FORCE)
set(USE_PANDOC OFF CACHE BOOL "" FORCE)
set(BUILD_EXECUTABLES OFF CACHE BOOL "" FORCE)
set(BUILD_FFI_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_POC OFF CACHE BOOL "" FORCE)
set(USE_QRCODEGEN OFF CACHE BOOL "" FORCE)
set(USE_GPM OFF CACHE BOOL "" FORCE)
set(USE_STATIC ON CACHE BOOL "" FORCE)
set(USE_MULTIMEDIA none CACHE STRING "" FORCE)

# Keep the parent test suite enabled while suppressing notcurses' own tests.
set(SM_TRACER_BUILD_TESTING ${BUILD_TESTING})
set(SM_TRACER_C_EXTENSIONS ${CMAKE_C_EXTENSIONS})
set(BUILD_TESTING OFF)
set(CMAKE_C_EXTENSIONS ON)
add_subdirectory(${NOTCURSES_ROOT} ${NOTCURSES_BINARY_DIR}
                 EXCLUDE_FROM_ALL)
set(BUILD_TESTING ${SM_TRACER_BUILD_TESTING})
set(CMAKE_C_EXTENSIONS ${SM_TRACER_C_EXTENSIONS})

set(NOTCURSES_INCLUDE_DIR
    ${NOTCURSES_ROOT}/include
    ${NOTCURSES_BINARY_DIR}/include)
set(NOTCURSES_LIBRARY notcurses-core-static)

#=============================================================================
# ===Component platform port.
if(WIN32)
    set(PLATFORM_SRC
        ${CMAKE_SOURCE_DIR}/ports/platform/platform_port_win32.c)
else()
    set(PLATFORM_SRC
        ${CMAKE_SOURCE_DIR}/ports/platform/platform_port_posix.c)
endif()
set(PLATFORM_INC ${CMAKE_SOURCE_DIR}/ports/platform)

add_library(platform_port STATIC ${PLATFORM_SRC})
target_include_directories(platform_port PUBLIC ${PLATFORM_INC})
if(NOT WIN32)
    find_package(Threads REQUIRED)
    target_link_libraries(platform_port PUBLIC Threads::Threads)
    target_compile_definitions(platform_port PRIVATE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
endif()

#=============================================================================
# ===Component libserialport.
set(SERIALPORT_ROOT ${CMAKE_SOURCE_DIR}/3rd_party/libserialport)
if(NOT WIN32)
    add_library(serialport SHARED IMPORTED GLOBAL)
    set(SERIALPORT_INCLUDE_DIR "${SERIALPORT_ROOT}/linux/include")
    set_target_properties(serialport PROPERTIES
        IMPORTED_LOCATION
            "${SERIALPORT_ROOT}/linux/libserialport.so"
        INTERFACE_INCLUDE_DIRECTORIES
            "${SERIALPORT_INCLUDE_DIR}"
    )
endif()

#=============================================================================
# ===Component yyjson.
set(YYJSON_ROOT ${CMAKE_SOURCE_DIR}/3rd_party/yyjson)
set(YYJSON_SRC ${YYJSON_ROOT}/src/yyjson.c)
set(YYJSON_INC ${YYJSON_ROOT}/src)

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
    ${YYJSON_SRC}
    ${BSP_SRC}
)

set(INC_ALL
    ${INC_LOCAL}
    ${BSP_INC}
    ${NOTCURSES_INCLUDE_DIR}
    ${SST_INC}
    ${PORT_INC}
    ${PLATFORM_INC}
    ${SM_HSM_INC}
    ${COMMON_C_INC}
    ${YYJSON_INC}
)

#=============================================================================
# ===Target rules.
add_executable(${PROJECT_NAME}
    ${SRC_ALL}
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${INC_ALL}
)

if(NOT WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        _DEFAULT_SOURCE
        _POSIX_C_SOURCE=200809L
        _XOPEN_SOURCE=700
    )
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${NOTCURSES_LIBRARY}
    platform_port
)
if(NOT WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE serialport)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/protocols
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/utils/protocols
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/protocols
)

if(NOT WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "${SERIALPORT_ROOT}/linux"
        INSTALL_RPATH_USE_LINK_PATH TRUE
    )
endif()

#=============================================================================
# ===Tests and compile-time interface contracts.
if(BUILD_TESTING)
    add_executable(platform_port_test
        tests/platform_port_test.c
    )
    target_link_libraries(platform_port_test PRIVATE platform_port)
    add_test(NAME platform_port COMMAND platform_port_test)

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
        application/aos/sp_mngr/protocol_catalog.c
        application/aos/sp_mngr/protocol_decoder.c
        application/aos/sp_mngr/protocol_frame_formatter.c
        ${YYJSON_SRC}
        ${SM_HSM_SRC}
    )
    target_include_directories(sp_mngr_command_test PRIVATE
        ${INC_ALL}
    )
    target_link_libraries(sp_mngr_command_test PRIVATE platform_port)
    target_compile_definitions(sp_mngr_command_test PRIVATE
        DBC_DISABLE
        SM_DBC_DISABLE
        _POSIX_C_SOURCE=200809L
    )
    add_custom_command(TARGET sp_mngr_command_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:sp_mngr_command_test>/protocols
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/utils/protocols
            $<TARGET_FILE_DIR:sp_mngr_command_test>/protocols
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

    add_executable(protocol_decoder_test
        tests/protocol_decoder_test.c
        application/aos/sp_mngr/protocol_decoder.c
        ${YYJSON_SRC}
    )
    target_include_directories(protocol_decoder_test PRIVATE
        ${INC_ALL}
    )
    target_link_libraries(protocol_decoder_test PRIVATE platform_port)
    target_compile_definitions(protocol_decoder_test PRIVATE
        DBC_DISABLE
    )
    add_test(NAME protocol_decoder
             COMMAND protocol_decoder_test
                     ${CMAKE_SOURCE_DIR}/utils/protocols/blinky_c51.json)

    add_executable(protocol_frame_formatter_test
        tests/protocol_frame_formatter_test.c
        application/aos/sp_mngr/protocol_frame_formatter.c
        application/aos/sp_mngr/protocol_decoder.c
        ${YYJSON_SRC}
    )
    target_include_directories(protocol_frame_formatter_test PRIVATE
        ${INC_ALL}
    )
    target_link_libraries(protocol_frame_formatter_test PRIVATE platform_port)
    target_compile_definitions(protocol_frame_formatter_test PRIVATE
        DBC_DISABLE
    )
    add_test(NAME protocol_frame_formatter
             COMMAND protocol_frame_formatter_test)

    add_executable(protocol_catalog_test
        tests/protocol_catalog_test.c
        application/aos/sp_mngr/protocol_catalog.c
    )
    target_include_directories(protocol_catalog_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(protocol_catalog_test PRIVATE
        DBC_DISABLE
        _POSIX_C_SOURCE=200809L
    )
    add_custom_command(TARGET protocol_catalog_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:protocol_catalog_test>/protocols
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/utils/protocols
            $<TARGET_FILE_DIR:protocol_catalog_test>/protocols
    )
    if(WIN32)
        add_test(NAME protocol_catalog
            COMMAND $<TARGET_FILE:protocol_catalog_test>
                    $<TARGET_FILE_DIR:protocol_catalog_test>/protocols)
    else()
        set(PROTOCOL_CATALOG_TEST_LINK
            ${CMAKE_CURRENT_BINARY_DIR}/catalog_shortcut/protocol_catalog_test)
        add_custom_command(TARGET protocol_catalog_test POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                ${CMAKE_CURRENT_BINARY_DIR}/catalog_shortcut
            COMMAND ${CMAKE_COMMAND} -E create_symlink
                $<TARGET_FILE:protocol_catalog_test>
                ${PROTOCOL_CATALOG_TEST_LINK}
        )
        add_test(NAME protocol_catalog
            COMMAND ${PROTOCOL_CATALOG_TEST_LINK}
                    $<TARGET_FILE_DIR:protocol_catalog_test>/protocols)
    endif()

    if(NOT WIN32)
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
        target_link_libraries(sp_thread_event_flow_test PRIVATE platform_port)
        add_test(NAME sp_thread_event_flow
                 COMMAND sp_thread_event_flow_test)
    endif()

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
    target_link_libraries(sp_thread_event_contract_test PRIVATE platform_port)
    add_test(NAME sp_thread_event_contract
             COMMAND sp_thread_event_contract_test)

    if(NOT WIN32)
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
        target_link_libraries(sp_thread_wake_test PRIVATE platform_port)
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
        target_link_libraries(serial_port_runtime_test PRIVATE platform_port)
        add_test(NAME serial_port_runtime
                 COMMAND serial_port_runtime_test)
    else()
        add_executable(serial_port_runtime_win32_test
            tests/serial_port_runtime_win32_test.c
            application/sp_thread/thread/serial_port_runtime.c
        )
        target_include_directories(serial_port_runtime_win32_test PRIVATE
            ${INC_ALL}
        )
        target_link_libraries(serial_port_runtime_win32_test PRIVATE
            platform_port
        )
        add_test(NAME serial_port_runtime_win32
                 COMMAND serial_port_runtime_win32_test)
    endif()

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
    target_link_libraries(ui_port_list_event_test PRIVATE platform_port)
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
    if(NOT WIN32)
        add_test(NAME ui_input_composer_lifecycle
                 COMMAND ui_input_composer_lifecycle_test)
        set_tests_properties(ui_input_composer_lifecycle PROPERTIES
            ENVIRONMENT "TERM=xterm-256color"
        )
    endif()

    add_executable(ui_command_suggestion_lifecycle_test
        tests/ui_command_suggestion_lifecycle.c
        application/ui/widgets/command_suggestion.c
        application/ui/widgets/selection_viewport_priv.c
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
    if(NOT WIN32)
        add_test(NAME ui_command_suggestion_lifecycle
                 COMMAND ui_command_suggestion_lifecycle_test)
        set_tests_properties(ui_command_suggestion_lifecycle PROPERTIES
            ENVIRONMENT "TERM=xterm-256color"
        )
    endif()

    add_executable(selection_viewport_test
        tests/selection_viewport_test.c
        application/ui/widgets/selection_viewport_priv.c
    )
    target_include_directories(selection_viewport_test PRIVATE
        ${INC_ALL}
    )
    target_compile_definitions(selection_viewport_test PRIVATE
        DBC_DISABLE
    )
    add_test(NAME selection_viewport
             COMMAND selection_viewport_test)

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
    if(NOT WIN32)
        add_test(NAME text_area
                 COMMAND text_area_test)
    endif()

    if(NOT WIN32)
        add_test(NAME ui_widget_boundary_contract
                 COMMAND ${CMAKE_SOURCE_DIR}/tests/ui_widget_boundary_contract.sh)

        add_test(NAME system_message_contract
                 COMMAND sh ${CMAKE_SOURCE_DIR}/tests/system_message_contract.sh)
    else()
        get_property(TEST_NAMES DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     PROPERTY TESTS)
        set_tests_properties(${TEST_NAMES} PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:${UCRT64_ROOT}/bin"
        )
    endif()
endif()

#=============================================================================
# ===Run helper target.
add_custom_target(run
    COMMAND $<TARGET_FILE:${PROJECT_NAME}>
    DEPENDS ${PROJECT_NAME}
    USES_TERMINAL
    VERBATIM
)
