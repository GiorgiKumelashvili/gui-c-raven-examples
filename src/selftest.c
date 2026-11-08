/*
 * Debug: APP_TEST=datatable|segmented|popover APP_SHOT=/tmp/x.tga ./build/raven_app
 * drives the window with injected input, then main.c captures it.
 */

#include <stdio.h>
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

static Rid find_type(Rid widget, RWidgetType type) {
	int i;
	if (Rwidget_get_type(widget) == type) return widget;
	for (i = 0; i < Rwidget_get_child_count(widget); i++) {
		Rid found = find_type(Rwidget_get_child(widget, i), type);
		if (found != R_INVALID_ID) return found;
	}
	return R_INVALID_ID;
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
	} else if (strncmp(name, "hover:", 6) == 0) {
		/* APP_TEST=hover:X,Y rests the pointer there */
		float hx = 0.0f, hy = 0.0f;
		sscanf(name + 6, "%f,%f", &hx, &hy);
		Rwindow_inject_mouse_move(g_window, hx - 10.0f, hy);
		pump(0.1);
		Rwindow_inject_mouse_move(g_window, hx, hy);
	} else if (strcmp(name, "omnikeys") == 0) {
		omnibar_toggle();
		pump(0.3);
		Rwindow_inject_text(g_window, "d");
		pump(0.2);
		/* as Cocoa sends them: the key, then its "character" NSDownArrowFunctionKey (U+F701) */
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_TRUE, 0);
		Rwindow_inject_text(g_window, "\xEF\x9C\x81");
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_FALSE, 0);
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_TRUE, 0);
		Rwindow_inject_text(g_window, "\xEF\x9C\x81");
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_FALSE, 0);
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_TRUE, 0);
		Rwindow_inject_text(g_window, "\xEF\x9C\x81");
		Rwindow_inject_key(g_window, R_KEY_DOWN, R_FALSE, 0);
	} else if (strcmp(name, "dockscroll") == 0) {
		/* the page scrolled to its last card */
		int i;
		nav_go("layout.dock");
		pump(0.3);
		Rwindow_inject_mouse_move(g_window, 700, 500);
		for (i = 0; i < 30; i++) {
			Rwindow_inject_mouse_wheel(g_window, 0.0f, -3.0f);
			pump(0.02);
		}
	} else if (strncmp(name, "rect:", 5) == 0) {
		/* APP_TEST=rect:NAME prints the rects of a widget and of its ancestors */
		Rid widget = Rwidget_find(g_window, name + 5);
		pump(0.5);
		while (widget != R_INVALID_ID) {
			Rwidget_get_rect(widget, &x, &y, &w, &h);
			fprintf(stderr, "[selftest] type %x  %.0f,%.0f  %.0f x %.0f\n", (unsigned)Rwidget_get_type(widget), x, y, w, h);
			widget = Rwidget_get_parent(widget);
		}
	} else if (strcmp(name, "treescroll") == 0) {
		/* the pointer rests on the sidebar, the wheel scrolls it: the hover must follow */
		int i;
		Rwindow_inject_mouse_move(g_window, 120, 400);
		pump(0.2);
		for (i = 0; i < 6; i++) {
			Rwindow_inject_mouse_wheel(g_window, 0.0f, -1.0f);
			pump(0.03);
		}
		pump(0.6);
	} else if (strcmp(name, "dockresize") == 0) {
		int i;
		nav_go("layout.dock");
		pump(0.3);
		Rwindow_inject_mouse_move(g_window, 700, 500);
		for (i = 0; i < 40; i++) {
			Rwindow_inject_mouse_wheel(g_window, 0.0f, -3.0f);
			pump(0.02);
		}
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "dock4_grip"), &x, &y, &w, &h);
		Rwindow_inject_mouse_move(g_window, x + w * 0.5f, y + h * 0.5f);
		pump(0.05);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_TRUE);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + w * 0.5f, y + h * 0.5f - 60.0f);
		pump(0.05);
		Rwindow_inject_mouse_move(g_window, x + w * 0.5f, y + h * 0.5f - 120.0f);
		pump(0.05);
		Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_FALSE);
		Rwidget_get_rect(Rwidget_find(g_window, "dock4_console"), &x, &y, &w, &h);
		fprintf(stderr, "[selftest] console height %.0f (was 160)\n", h);
		Rwidget_get_rect(Rwidget_find(g_window, "dock4_editor"), &x, &y, &w, &h);
		fprintf(stderr, "[selftest] editor height %.0f\n", h);
	} else if (strcmp(name, "speeds") == 0) {
		int i, c;
		nav_go("custom.scrollspeed");
		pump(0.4);
		for (c = 0; c < 5; c++) {
			Rwindow_inject_mouse_move(g_window, 300 + 200 + (float)c * 190.0f, 400);
			pump(0.05);
			for (i = 0; i < 4; i++) {
				Rwindow_inject_mouse_wheel(g_window, 0.3f, -2.0f);
				pump(0.02);
			}
		}
		Rwindow_inject_mouse_move(g_window, 1300, 400);
	} else if (strcmp(name, "contain") == 0) {
		/* the pointer on a code editor of a long page: scroll far past the editor's end;
		 * the page must not move. Then outside of it: the page moves. */
		int i;
		float px, py, py0, page_x, page_y, pw, ph;
		Rid page, editor;
		nav_go("custom.scrollspeed");
		pump(0.4);
		page = Rwidget_find(g_window, "page.custom.scrollspeed");
		editor = find_type(page, R_WIDGET_TEXT_EDITOR);
		Rwidget_get_rect(page, &page_x, &page_y, &pw, &ph);
		Rwidget_get_rect(editor, &x, &y, &w, &h);
		Rscroll_box_set_offset(page, 0.0f, y - page_y - 120.0f);
		pump(0.6);
		Rscroll_box_get_offset(page, &px, &py0);
		Rwidget_get_rect(editor, &x, &y, &w, &h);
		Rwindow_inject_mouse_move(g_window, x + w * 0.5f, y + h * 0.5f);
		pump(0.1);
		for (i = 0; i < 60; i++) {
			Rwindow_inject_mouse_wheel(g_window, 0.0f, -2.0f);
			pump(0.02);
		}
		pump(0.4);
		Rscroll_box_get_offset(page, &px, &py);
		fprintf(stderr, "[selftest] page offset %.0f -> %.0f after 60 wheel steps over the editor\n", py0, py);
		py0 = py;
		Rwindow_inject_mouse_move(g_window, page_x + pw - 20.0f, y + h * 0.5f);
		pump(0.1);
		for (i = 0; i < 5; i++) {
			Rwindow_inject_mouse_wheel(g_window, 0.0f, -2.0f);
			pump(0.02);
		}
		pump(0.4);
		Rscroll_box_get_offset(page, &px, &py);
		fprintf(stderr, "[selftest] page offset %.0f -> %.0f after 5 steps beside it\n", py0, py);
	} else if (strcmp(name, "multiwindow") == 0) {
		/* two windows, then increments from the page and from the windows */
		Rid page;
		nav_go("custom.multiwindow");
		pump(0.3);
		page = Rwidget_find(g_window, "pages");
		Raction_fire("mw.open", page);
		Raction_fire("mw.open", page);
		pump(0.5);
		Raction_fire("mw.increment", page);
		Raction_fire("mw.increment", page);
		Raction_fire("mw.increment", page);
		pump(0.3);
		fprintf(stderr, "[selftest] page shows %s, %s\n", Rlabel_get_text(Rwidget_find(g_window, "mw_count")),
			Rlabel_get_text(Rwidget_find(g_window, "mw_status")));
		{
			/* the button of the second window, clicked for real: the action comes from that window */
			Rid second = multiwindow_window(1), button;
			int i;
			button = find_type(Rwindow_get_content(second), R_WIDGET_BUTTON);
			Rwidget_get_rect(button, &x, &y, &w, &h);
			Rwindow_inject_mouse_move(second, x + w * 0.5f, y + h * 0.5f);
			Rwindow_inject_mouse_button(second, R_MOUSE_LEFT, R_TRUE);
			Rwindow_inject_mouse_button(second, R_MOUSE_LEFT, R_FALSE);
			pump(0.3);
			for (i = 0; i < multiwindow_count(); i++) {
				fprintf(stderr, "[selftest] window %d shows %s, %s\n", i + 1, Rlabel_get_text(Rwidget_find(multiwindow_window(i), "mw_count")),
					Rlabel_get_text(Rwidget_find(multiwindow_window(i), "mw_status")));
			}
			fprintf(stderr, "[selftest] page shows %s\n", Rlabel_get_text(Rwidget_find(g_window, "mw_count")));
		}
	} else if (strcmp(name, "neo") == 0) {
		nav_go("custom.accordion");
		pump(0.3);
		Raction_fire("neo:1", Rwidget_find(g_window, "pages"));
	} else if (strcmp(name, "http") == 0) {
		nav_go("custom.http");
		pump(0.3);
		Raction_fire("http.send", Rwidget_find(g_window, "pages"));
		pump(4.0);
	} else if (strcmp(name, "vl1m") == 0) {
		Rid combo;
		nav_go("custom.virtual");
		pump(0.3);
		combo = Rwidget_find(g_window, "vl_count_combo");
		if (combo != R_INVALID_ID) {
			Rcombo_box_set_selected(combo, 2);
			Raction_fire("vl.count", combo);
		}
		pump(0.3);
		Rwidget_focus(Rwidget_find(g_window, "vl_filter"));
		Rwindow_inject_text(g_window, "ada tokyo");
		pump(0.5);
		fprintf(stderr, "[selftest] %s\n", Rlabel_get_text(Rwidget_find(g_window, "vl_stats")));
	} else if (strncmp(name, "action:", 7) == 0) {
		/* APP_TEST=action:notify.rich fires an action, then prints the labels named by APP_TEST_LABEL */
		Raction_fire(name + 7, Rwidget_find(g_window, "pages"));
		pump(3.0);
		if (getenv("APP_TEST_LABEL")) {
			Rid label = Rwidget_find(g_window, getenv("APP_TEST_LABEL"));
			fprintf(stderr, "[selftest] %s = %s\n", getenv("APP_TEST_LABEL"), label != R_INVALID_ID ? Rlabel_get_text(label) : "(none)");
		}
	} else if (strcmp(name, "popover") == 0) {
		nav_go("custom.popover");
		pump(0.5);
		Rwidget_get_rect(Rwidget_find(g_window, "pop_filters"), &x, &y, &w, &h);
		click_at(x + w * 0.5f, y + h * 0.5f);
	}
	pump(1.0);
}
