cmake_minimum_required(VERSION 3.16)
project(raven_app C)

if(APPLE)
	enable_language(OBJC)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_subdirectory(third_party/Raven)

set(APP_SOURCES
	src/main.c
	src/nav.c
	src/demos.c
	src/popover.c
	src/datatable.c
	src/segmented.c
	src/selftest.c
)

if(APPLE)
	list(APPEND APP_SOURCES src/native_menu_macos.m)
else()
	list(APPEND APP_SOURCES src/native_menu_stub.c)
endif()

add_executable(raven_app ${APP_SOURCES})
target_link_libraries(raven_app PRIVATE raven::raven)

if(APPLE)
	target_link_libraries(raven_app PRIVATE "-framework AppKit")
	set_source_files_properties(src/native_menu_macos.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

# where main.xml lives, so live reload follows the source file and not a copy
target_compile_definitions(raven_app PRIVATE APP_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")
