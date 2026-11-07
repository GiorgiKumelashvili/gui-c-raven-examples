/*
 * Debug: APP_TEST=datatable|segmented|popover APP_SHOT=/tmp/x.tga ./build/raven_app
 * drives the window with injected input, then main.c captures it.
 */

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
	} else if (strcmp(name, "popover") == 0) {
		nav_go("custom.popover");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "pop_filters"), &x, &y, &w, &h);
		click_at(x + w * 0.5f, y + h * 0.5f);
	}
	pump(1.0);
}
