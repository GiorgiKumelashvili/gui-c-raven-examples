/*
 * A popover: a floating panel anchored under the widget that opened it.
 *
 * Raven has popups for its menus, lists and pickers, but no popover that
 * holds any widget. This one lives in main.xml: a Canvas laid over the whole
 * window (a Canvas lets the mouse through where it has no child), holding a
 * Border named "popover" whose children are the possible contents. Opening
 * shows the content asked for and places the Border under the anchor; a
 * click elsewhere, or Escape, closes it.
 */

#include <string.h>
#include "app.h"

#define POPOVER_WIDTH 290.0f /* Width of the Border "popover" in main.xml */
#define POPOVER_MARGIN 16.0f

static Rid g_anchor = R_INVALID_ID;

static Rid popover_widget(void) {
	return Rwidget_find(g_window, "popover");
}

static int popover_open(void) {
	Rid pop = popover_widget();
	return pop != R_INVALID_ID && g_anchor != R_INVALID_ID && Rwidget_get_visibility(pop) == R_VISIBLE;
}

void popover_close(void) {
	Rid pop = popover_widget();
	if (pop != R_INVALID_ID) Rwidget_set_visibility(pop, R_COLLAPSED);
	g_anchor = R_INVALID_ID;
}

static int inside(Rid widget, float x, float y) {
	float rx, ry, rw, rh;
	if (widget == R_INVALID_ID) return 0;
	Rwidget_get_rect(widget, &rx, &ry, &rw, &rh);
	return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

void popover_toggle(Rid anchor, const char* content_name) {
	Rid pop = popover_widget();
	Rid layer = Rwidget_find(g_window, "popover_layer");
	Rid body;
	float ax, ay, aw, ah, lx, ly, lw, lh, pw, ph, x, y;
	int i, count;

	if (pop == R_INVALID_ID || layer == R_INVALID_ID) return;
	if (popover_open() && g_anchor == anchor) {
		popover_close();
		return;
	}

	/* show the content asked for, hide the others */
	body = Rwidget_get_child(pop, 0);
	count = Rwidget_get_child_count(body);
	for (i = 0; i < count; i++) {
		Rid child = Rwidget_get_child(body, i);
		const char* name = Rwidget_get_name(child);
		Rwidget_set_visibility(child, name && strcmp(name, content_name) == 0 ? R_VISIBLE : R_COLLAPSED);
	}

	/* under the anchor, centered on it, kept inside the window */
	Rwidget_get_rect(anchor, &ax, &ay, &aw, &ah);
	Rwidget_get_rect(layer, &lx, &ly, &lw, &lh);
	/* a collapsed widget was never measured: fall back on the Width given in main.xml */
	Rwidget_get_desired_size(pop, &pw, &ph);
	if (pw < POPOVER_WIDTH) pw = POPOVER_WIDTH;
	x = ax + aw * 0.5f - pw * 0.5f - lx;
	y = ay + ah + 8.0f - ly;
	if (x + pw > lw - POPOVER_MARGIN) x = lw - POPOVER_MARGIN - pw;
	if (x < POPOVER_MARGIN) x = POPOVER_MARGIN;
	if (ph > 0.0f && y + ph > lh - 12.0f) y = ay - ly - ph - 8.0f; /* no room below: above */
	Rslot_set_anchors(pop, 0.0f, 0.0f, 0.0f, 0.0f);
	Rslot_set_alignment(pop, 0.0f, 0.0f);
	Rslot_set_position(pop, x, y);
	Rwidget_set_visibility(pop, R_VISIBLE);
	g_anchor = anchor;
}

static int popover_hook(const REvent* event) {
	if (!popover_open()) return R_FALSE;
	if (event->type == R_EVENT_MOUSE_DOWN) {
		/* inside: the popover handles it; on the anchor: its action toggles */
		if (inside(popover_widget(), event->x, event->y) || inside(g_anchor, event->x, event->y)) return R_FALSE;
		/* while a menu or a list of the popover is open, the click is theirs */
		if (Rwindow_has_popup(g_window)) return R_FALSE;
		popover_close();
		return R_FALSE;
	}
	if (event->type == R_EVENT_KEY_DOWN && event->key == R_KEY_ESCAPE) {
		popover_close();
		return R_TRUE;
	}
	return R_FALSE;
}

void popover_init(void) {
	app_add_event_hook(popover_hook);
}
