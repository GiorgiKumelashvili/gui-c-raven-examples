/* Other systems: none of the macOS services; the callers fall back on Raven. */
#include "app.h"

int native_context_menu(Rid window, float x, float y, const char* const* items, int count) {
	(void)window; (void)x; (void)y; (void)items; (void)count;
	return -2;
}
int native_open_panel(Rid window, const char* kind, NativePathsCallback callback, void* user_data) {
	(void)window; (void)kind; (void)callback; (void)user_data;
	return R_FALSE;
}
int native_save_panel(Rid window, const char* file_name, NativePathsCallback callback, void* user_data) {
	(void)window; (void)file_name; (void)callback; (void)user_data;
	return R_FALSE;
}
int native_reveal_in_finder(const char* path) { (void)path; return R_FALSE; }
int native_open_in_finder(const char* folder) { (void)folder; return R_FALSE; }
int native_notify(const char* title, const char* subtitle, const char* body, int sound, double delay,
	NativeNotifyCallback callback, void* user_data) {
	(void)subtitle; (void)sound; (void)delay;
	/* Raven's own: the notification of the system where there is one */
	if (callback) callback(Rnotification_show(title, body, R_NOTIFICATION_NORMAL) ? "Shown by Raven" : "No notifications here", user_data);
	return R_TRUE;
}
int native_dock_badge(const char* text) { (void)text; return R_FALSE; }
int native_request_attention(void) { return R_FALSE; }
