add_subdirectory(${PROJECT_SOURCE_DIR}/deps/tusb_xinput xinput_host)

add_executable(wirecutter
	main.c
	xinput.c
	gamepad.c
	tusb_config.h
	btstack_config.h
)

target_include_directories(wirecutter PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

pico_enable_stdio_usb(wirecutter 0)
pico_enable_stdio_uart(wirecutter 1)

pico_add_extra_outputs(wirecutter)

target_link_libraries(wirecutter PUBLIC
	pico_stdlib
	pico_cyw43_arch_none
	pico_btstack_cyw43
	pico_btstack_classic
	tinyusb_host
	tinyusb_board
	xinput_host
)

