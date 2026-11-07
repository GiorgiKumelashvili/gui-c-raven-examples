/*
 * A modal of the app, for contents that must reach the edges.
 *
 * Raven's Rdialog_open lays the content on a surface of its own, with a
 * margin that cannot be changed. This one shows the content as it is: a
 * dimmed layer over the window (main.xml, "modal_scrim") and the content in
 * the middle ("modal_host"), so a Border with Padding="0" is the whole card.
 * A click outside or Escape closes it, as a dialog of Raven.
 */

#include "app.h"

static Rid host(void) {
	return Rwidget_find(g_window, "modal_host");
}

int app_modal_is_open(void) {
	Rid h = host();
	return h != R_INVALID_ID && Rwidget_get_visibility(h) == R_VISIBLE;
}

int app_modal_contains(Rid widget) {
	Rid h = host();
	for (; widget != R_INVALID_ID; widget = Rwidget_get_parent(widget)) {
		if (widget == h) return R_TRUE;
	}
	return R_FALSE;
}

void app_modal_close(void) {
	Rid h = host();
	if (h == R_INVALID_ID) return;
	Rwidget_clear_children(h);
	Rwidget_set_visibility(h, R_COLLAPSED);
	Rwidget_set_visibility(Rwidget_find(g_window, "modal_scrim"), R_COLLAPSED);
}

int app_modal_open(const char* path) {
	Rid h = host();
	Rid content;
	if (h == R_INVALID_ID) return R_FALSE;
	content = Rwidget_create_from_xml_file(path);
	if (content == R_INVALID_ID) return R_FALSE;
	popover_close();
	Rwidget_clear_children(h);
	Rwidget_add_child(h, content);
	Rwidget_set_visibility(Rwidget_find(g_window, "modal_scrim"), R_VISIBLE);
	Rwidget_set_visibility(h, R_VISIBLE);
	return R_TRUE;
}

static int modal_hook(const REvent* event) {
	float x, y, w, hgt;
	if (!app_modal_is_open()) return R_FALSE;
	if (event->type == R_EVENT_KEY_DOWN && event->key == R_KEY_ESCAPE) {
		app_modal_close();
		return R_TRUE;
	}
	if (event->type == R_EVENT_MOUSE_DOWN) {
		Rwidget_get_rect(host(), &x, &y, &w, &hgt);
		if (event->x < x || event->y < y || event->x >= x + w || event->y >= y + hgt) {
			/* a list or a menu of the content takes its own clicks */
			if (Rwindow_has_popup(g_window)) return R_FALSE;
			app_modal_close();
			return R_TRUE;
		}
	}
	return R_FALSE;
}

void app_modal_init(void) {
	app_add_event_hook(modal_hook);
}
