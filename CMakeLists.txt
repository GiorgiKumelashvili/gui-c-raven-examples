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
	src/reorder.c
	src/omnibar.c
	src/modal.c
	src/tooltip.c
	src/vlist.c
	src/canvas_demo.c
	src/http.c
	src/roundlist.c
	src/selftest.c
)

if(APPLE)
	list(APPEND APP_SOURCES src/native_menu_macos.m src/native_macos.m)
else()
	list(APPEND APP_SOURCES src/native_menu_stub.c)
endif()

add_executable(raven_app ${APP_SOURCES})
target_link_libraries(raven_app PRIVATE raven::raven)

if(APPLE)
	target_link_libraries(raven_app PRIVATE "-framework AppKit" "-framework UserNotifications" "-framework UniformTypeIdentifiers")
	set_source_files_properties(src/native_menu_macos.m src/native_macos.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")

	# An application bundle (raven_app.app): the Notification Center and the
	# Dock only serve bundles with an identifier. Signed ad hoc so that macOS
	# keeps the permission given to its notifications.
	set_target_properties(raven_app PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_BUNDLE_NAME "Raven Components"
		MACOSX_BUNDLE_GUI_IDENTIFIER "dev.raven.components"
		MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Info.plist.in")
	add_custom_command(TARGET raven_app POST_BUILD
		COMMAND codesign --force --sign - "$<TARGET_BUNDLE_DIR:raven_app>"
		COMMENT "Signing raven_app.app ad hoc")
endif()

# where main.xml lives, so live reload follows the source file and not a copy
target_compile_definitions(raven_app PRIVATE APP_UI_DIR="${CMAKE_CURRENT_SOURCE_DIR}/src")
