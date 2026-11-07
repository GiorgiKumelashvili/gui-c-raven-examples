/*
 * Debug: APP_TEST=datatable|segmented|popover APP_SHOT=/tmp/x.tga ./build/raven_app
 * drives the window with injected input, then main.c captures it.
 */

#include <stdlib.h>
#include <string.h>
#include "app.h"

static void pump(double seconds) {
	double start = Rget_time();
	while (Rget_time() - start < seconds && Rwait_frame(0.016)) {
	}
}

static void click_at(float x, float y) {
	Rwindow_inject_mouse_move(g_window, x, y);
	pump(0.05);
	Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_TRUE);
	pump(0.05);
	Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_FALSE);
	pump(0.15);
}

void selftest_run(const char* name) {
	float x, y, w, h;
	pump(1.0);
	if (strcmp(name, "datatable") == 0) {
		nav_go("custom.datatable");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "datatable"), &x, &y, &w, &h);
		/* drag the edge between ID and Service 120 units to the right */
		Rwindow_inject_mouse_move(g_window, x + 64 + 70, y + 18);
		pump(0.1);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_TRUE);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + 64 + 130, y + 18);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + 64 + 190, y + 18);
		pump(0.05);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_FALSE);
		pump(0.1);
		/* check rows 1 and 3, unfold row 2, sort by Status (header, after the wider ID) */
		click_at(x + 300, y + 36 + 18);
		click_at(x + 300, y + 36 + 36 * 2 + 18);
		click_at(x + 18, y + 36 + 36 + 18);
		click_at(x + 64 + 190 + 170 + 130 + 30, y + 18);
	} else if (strcmp(name, "segmented") == 0) {
		nav_go("custom.segmented");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "seg_views"), &x, &y, &w, &h);
		/* the control sits above the switcher: its second segment */
		click_at(x + 130, y - 30);
	} else if (strcmp(name, "profile") == 0) {
		nav_go("custom.popover");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "pop_profile"), &x, &y, &w, &h);
		click_at(x + w * 0.5f, y + h * 0.5f);
	} else if (strcmp(name, "omni") == 0) {
		omnibar_toggle();
		pump(0.3);
		Rwindow_inject_text(g_window, "ta");
		pump(0.2);
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_TRUE, 0);
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_FALSE, 0);
	} else if (strcmp(name, "rowdrag") == 0) {
		nav_go("custom.datatable");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "datatable"), &x, &y, &w, &h);
		/* grip of row 0 down below row 3 */
		Rwindow_inject_mouse_move(g_window, x + 11, y + 36 + 18);
		pump(0.1);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_TRUE);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + 60, y + 36 + 36 * 2);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + 60, y + 36 + 36 * 3 + 30);
		pump(0.3);
		if (getenv("APP_TEST_HOLD")) return; /* the shot shows the drag */
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_FALSE);
	} else if (strcmp(name, "listdrag") == 0 || strcmp(name, "tabledrag") == 0) {
		int list = strcmp(name, "listdrag") == 0;
		float row = Rtheme_get_metric(R_METRIC_ROW_HEIGHT), top;
		nav_go(list ? "input.choice" : "data.table");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, list ? "reorder_list" : "raven_table"), &x, &y, &w, &h);
		top = y + 1 + (list ? 0 : row + 4);
		/* row 0 to row 3 */
		Rwindow_inject_mouse_move(g_window, x + 40, top + row * 0.5f);
		pump(0.05);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_TRUE);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + 40, top + row * 2.0f);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + 40, top + row * 3.5f);
		pump(0.1);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_FALSE);
	} else if (strcmp(name, "popover") == 0) {
		nav_go("custom.popover");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "pop_filters"), &x, &y, &w, &h);
		click_at(x + w * 0.5f, y + h * 0.5f);
	}
	pump(1.0);
}
