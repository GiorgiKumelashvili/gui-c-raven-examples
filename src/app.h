#ifndef RAVEN_APP_H
#define RAVEN_APP_H

#include <Raven.h>

#ifndef APP_UI_DIR
#define APP_UI_DIR "."
/* native_macos.m: services of macOS that Raven does not wrap (stubs elsewhere).
 * Each returns R_FALSE when the system has none. Callbacks run on the UI thread. */
typedef void (*NativePathsCallback)(int count, const char* const* paths, void* user_data);
typedef void (*NativeNotifyCallback)(const char* status, void* user_data);
/* kind: NULL or "" any file, "images", "documents", "multi", "folder" */
int native_open_panel(Rid window, const char* kind, NativePathsCallback callback, void* user_data);
int native_save_panel(Rid window, const char* file_name, NativePathsCallback callback, void* user_data);
int native_reveal_in_finder(const char* path);
int native_open_in_finder(const char* folder);
int native_notify(const char* title, const char* subtitle, const char* body, int sound, double delay,
	NativeNotifyCallback callback, void* user_data);
int native_dock_badge(const char* text);
int native_request_attention(void);

/* reorder.c: rows of a ListView or a Table dragged to another place, or moved with Alt+Up/Down */
void reorder_init(void);
void reorder_frame(void);

/* omnibar.c: the command palette (Cmd+K) and the omni bar page */
void omnibar_init(void);
void omnibar_bind_widgets(void);
void omnibar_toggle(void);

/* nav.c: the pages, for the omni bar; history */
int nav_entry_count(void);
const char* nav_entry_id(int index);
const char* nav_entry_title(int index);
const char* nav_entry_section(int index);
void nav_back(void);
void nav_forward(void);

/* selftest.c: debug, drives the window with injected input (APP_TEST) */
void selftest_run(const char* name);

#endif

/* The main window. Widgets are looked up by name each time they are needed:
 * a live reload of the XML may have rebuilt them. */
extern Rid g_window;

/* Shows a short message in the toast of the window. */
void app_toast(const char* text, RToastKind kind);

/* Window event hooks: the window has one hook; the parts of the app that
 * need to see raw input (popover, data table column drag) chain here. */
typedef int (*AppEventHook)(const REvent* event);
void app_add_event_hook(AppEventHook hook);

/* nav.c: the sidebar tree and the page switcher */
void nav_init(void);
void nav_rebuild(const char* filter);
void nav_go(const char* page);

/* demos.c: actions of the demo pages */
void demos_init(void);
void demos_bind_widgets(void);

/* popover.c: a floating panel anchored to a widget */
void popover_init(void);
void popover_toggle(Rid anchor, const char* content_name);
void popover_close(void);

/* datatable.c: <DataTable/> tag */
void datatable_register(void);
int datatable_checked_count(Rid table);
int datatable_row_count(Rid table);
/* "check-all", "check-none", "invert", "expand-all", "collapse-all", "reset-columns" */
void datatable_command(Rid table, const char* command);

/* segmented.c: <SegmentedControl/> tag */
void segmented_register(void);
int segmented_get_selected(Rid control);
const char* segmented_get_item(Rid control, int index);

/* native_menu_*.{m,c}: the context menu of the operating system.
 * Returns the index chosen, -1 when dismissed, -2 when not supported. */
int native_context_menu(Rid window, float x, float y, const char* const* items, int count);

/* native_macos.m: services of macOS that Raven does not wrap (stubs elsewhere).
 * Each returns R_FALSE when the system has none. Callbacks run on the UI thread. */
typedef void (*NativePathsCallback)(int count, const char* const* paths, void* user_data);
typedef void (*NativeNotifyCallback)(const char* status, void* user_data);
/* kind: NULL or "" any file, "images", "documents", "multi", "folder" */
int native_open_panel(Rid window, const char* kind, NativePathsCallback callback, void* user_data);
int native_save_panel(Rid window, const char* file_name, NativePathsCallback callback, void* user_data);
int native_reveal_in_finder(const char* path);
int native_open_in_finder(const char* folder);
int native_notify(const char* title, const char* subtitle, const char* body, int sound, double delay,
	NativeNotifyCallback callback, void* user_data);
int native_dock_badge(const char* text);
int native_request_attention(void);

/* reorder.c: rows of a ListView or a Table dragged to another place, or moved with Alt+Up/Down */
void reorder_init(void);
void reorder_frame(void);

/* omnibar.c: the command palette (Cmd+K) and the omni bar page */
void omnibar_init(void);
void omnibar_bind_widgets(void);
void omnibar_toggle(void);

/* nav.c: the pages, for the omni bar; history */
int nav_entry_count(void);
const char* nav_entry_id(int index);
const char* nav_entry_title(int index);
const char* nav_entry_section(int index);
void nav_back(void);
void nav_forward(void);

/* selftest.c: debug, drives the window with injected input (APP_TEST) */
void selftest_run(const char* name);

#endif
