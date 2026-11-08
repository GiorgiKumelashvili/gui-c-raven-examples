/*
 * Several windows, one state.
 *
 * The count lives here, in C, once. The page shows it and so does every
 * window opened from it (src/windows/counter.xml); each has a button that
 * adds one. After a change, every place that shows the count is written
 * again: the label of the page, and the label of each window still open.
 * Windows are separate Raven windows, built from their own XML; their
 * buttons fire the same action as the one of the page.
 */

#include <stdio.h>
#include <string.h>
#include "app.h"

#define MAX_WINDOWS 16

static int g_count;
static Rid g_windows[MAX_WINDOWS];
static int g_window_count;
static int g_opened; /* windows opened so far: their number and their place */

/* The windows closed by the user are gone: forget them. */
static void prune(void) {
	int i, kept = 0;
	for (i = 0; i < g_window_count; i++) {
		if (Rwindow_is_valid(g_windows[i])) g_windows[kept++] = g_windows[i];
	}
	g_window_count = kept;
}

static void show(void) {
	char number[32], status[96];
	int i;
	prune();
	snprintf(number, sizeof(number), "%d", g_count);
	snprintf(status, sizeof(status), g_window_count == 1 ? "1 window open" : "%d windows open", g_window_count);
	if (Rwidget_find(g_window, "mw_count") != R_INVALID_ID) Rlabel_set_text(Rwidget_find(g_window, "mw_count"), number);
	if (Rwidget_find(g_window, "mw_status") != R_INVALID_ID) Rlabel_set_text(Rwidget_find(g_window, "mw_status"), status);
	for (i = 0; i < g_window_count; i++) {
		Rid label = Rwidget_find(g_windows[i], "mw_count");
		Rid other = Rwidget_find(g_windows[i], "mw_status");
		if (label != R_INVALID_ID) Rlabel_set_text(label, number);
		if (other != R_INVALID_ID) Rlabel_set_text(other, status);
	}
}

/* A window closes: the others say how many are left (on the next frame, once it is gone). */
static void on_closed_later(void* user_data) {
	(void)user_data;
	show();
}

static int on_close(Rid window, void* user_data) {
	(void)window;
	(void)user_data;
	Rtask_post(on_closed_later, NULL);
	return R_TRUE;
}

static void on_action(Rid sender, const char* action, void* user_data) {
	(void)sender;
	(void)user_data;
	if (strcmp(action, "mw.increment") == 0) {
		g_count++;
	} else if (strcmp(action, "mw.reset") == 0) {
		g_count = 0;
	} else if (strcmp(action, "mw.open") == 0) {
		Rid window;
		char title[64];
		int x = 0, y = 0;
		prune();
		if (g_window_count == MAX_WINDOWS) {
			app_toast("Enough windows for one demo.", R_TOAST_WARNING);
			return;
		}
		window = Rwindow_create_from_xml_file(APP_UI_DIR "/windows/counter.xml");
		if (window == R_INVALID_ID) return;
		g_opened++;
		snprintf(title, sizeof(title), "Counter %d", g_opened);
		Rwindow_set_title(window, title);
		/* beside the main window, each one a little lower than the last */
		Rwindow_get_position(g_window, &x, &y);
		Rwindow_set_position(window, x + 80 + 36 * ((g_opened - 1) % 8), y + 80 + 36 * ((g_opened - 1) % 8));
		Rwindow_set_close_callback(window, on_close, NULL);
		native_live_resize_enable(window);
		g_windows[g_window_count++] = window;
	} else if (strcmp(action, "mw.close_all") == 0) {
		int i;
		prune();
		for (i = 0; i < g_window_count; i++) Rwindow_destroy(g_windows[i]);
		g_window_count = 0;
	}
	show();
}

/* Closing the main window quits, even while counters are open. */
static int on_main_close(Rid window, void* user_data) {
	(void)window;
	(void)user_data;
	Rquit(0);
	return R_TRUE;
}

/* For the self-test: the windows open. */
int multiwindow_count(void) {
	prune();
	return g_window_count;
}

Rid multiwindow_window(int index) {
	return index >= 0 && index < g_window_count ? g_windows[index] : R_INVALID_ID;
}

void multiwindow_init(void) {
	Raction_bind("mw.increment", on_action, NULL);
	Raction_bind("mw.reset", on_action, NULL);
	Raction_bind("mw.open", on_action, NULL);
	Raction_bind("mw.close_all", on_action, NULL);
}

void multiwindow_bind_widgets(void) {
	Rwindow_set_close_callback(g_window, on_main_close, NULL);
	show();
}
