/*
 * Tooltips, closer to the pointer than Raven's.
 *
 * Raven draws a tooltip 12 units right of and 22 units below the point
 * where the pointer started resting, and this cannot be changed. This file
 * takes the Tooltip= texts of the window over: it reads them, clears them
 * (so that Raven shows nothing) and shows its own Border "app_tooltip"
 * (main.xml) at the pointer, after the same short rest. Moving to another
 * widget while one is shown switches at once, as on macOS.
 */

#include <stdlib.h>
#include <string.h>
#include "app.h"

#define TIP_DELAY 0.55
#define TIP_QUICK 0.35  /* after a tooltip just went: the next one comes at once */
#define TIP_OFFSET_X 4.0f
#define TIP_OFFSET_Y 17.0f

typedef struct Tip {
	Rid widget;
	char* text;
} Tip;

static Tip* g_tips;
static int g_tip_count, g_tip_capacity;
static Rid g_hovered = R_INVALID_ID;
static Rid g_timer = R_INVALID_ID;
static float g_x, g_y;
static int g_shown;
static int g_suppressed;
static double g_hidden_at = -10.0;

static const char* tip_text(Rid widget) {
	int i;
	for (i = 0; i < g_tip_count; i++) {
		if (g_tips[i].widget == widget) return g_tips[i].text;
	}
	return NULL;
}

static void take(Rid widget) {
	const char* text = Rwidget_get_tooltip(widget);
	int i;
	if (!text || !text[0]) return;
	for (i = 0; i < g_tip_count; i++) {
		if (g_tips[i].widget == widget) break;
	}
	if (i == g_tip_count) {
		if (g_tip_count == g_tip_capacity) {
			g_tip_capacity = g_tip_capacity ? g_tip_capacity * 2 : 64;
			g_tips = (Tip*)realloc(g_tips, sizeof(Tip) * (size_t)g_tip_capacity);
		}
		g_tips[g_tip_count].widget = widget;
		g_tips[g_tip_count].text = NULL;
		g_tip_count++;
	}
	free(g_tips[i].text);
	g_tips[i].text = strdup(text);
	Rwidget_set_tooltip(widget, NULL);
}

static void scan(Rid widget) {
	int i, n;
	if (widget == R_INVALID_ID) return;
	take(widget);
	n = Rwidget_get_child_count(widget);
	for (i = 0; i < n; i++) scan(Rwidget_get_child(widget, i));
}

static void hide(void) {
	Rid tip = Rwidget_find(g_window, "app_tooltip");
	if (g_timer != R_INVALID_ID && Rtimer_is_valid(g_timer)) Rtimer_destroy(g_timer);
	g_timer = R_INVALID_ID;
	if (g_shown) g_hidden_at = Rget_time();
	g_shown = 0;
	if (tip != R_INVALID_ID) Rwidget_set_visibility(tip, R_COLLAPSED);
}

static void show(void) {
	Rid tip = Rwidget_find(g_window, "app_tooltip");
	Rid label = Rwidget_find(g_window, "app_tooltip_text");
	Rid layer = Rwidget_find(g_window, "popover_layer");
	const char* text = tip_text(g_hovered);
	float lx, ly, lw, lh, tw, th, x, y, r, g, b, a;
	if (tip == R_INVALID_ID || label == R_INVALID_ID || layer == R_INVALID_ID || !text) return;

	/* the colors of the tooltips of the theme */
	Rwidget_get_color(tip, R_COLOR_TOOLTIP, &r, &g, &b, &a);
	Rborder_set_background(tip, r, g, b, a < 0.9f ? 0.96f : a);
	Rwidget_get_color(tip, R_COLOR_TOOLTIP_TEXT, &r, &g, &b, &a);
	Rlabel_set_color(label, r, g, b, a);
	Rlabel_set_text(label, text);

	Rwidget_get_rect(layer, &lx, &ly, &lw, &lh);
	Rfont_measure_text(Rtheme_get_font(), 12.0f, text, &tw, &th);
	tw += 18.0f;
	th += 10.0f;
	x = g_x + TIP_OFFSET_X - lx;
	y = g_y + TIP_OFFSET_Y - ly;
	if (x + tw > lw - 4.0f) x = lw - 4.0f - tw;
	if (x < 4.0f) x = 4.0f;
	if (y + th > lh - 4.0f) y = g_y - ly - th - 6.0f; /* no room below: above the pointer */
	Rslot_set_anchors(tip, 0.0f, 0.0f, 0.0f, 0.0f);
	Rslot_set_position(tip, x, y);
	Rwidget_set_visibility(tip, R_VISIBLE);
	g_shown = 1;
}

static void on_timer(Rid timer, void* user_data) {
	(void)timer;
	(void)user_data;
	g_timer = R_INVALID_ID;
	if (g_hovered != R_INVALID_ID && !g_suppressed) show();
}

static Rid owner_of(Rid widget) {
	for (; widget != R_INVALID_ID; widget = Rwidget_get_parent(widget)) {
		if (tip_text(widget)) return widget;
	}
	return R_INVALID_ID;
}

static int tooltip_hook(const REvent* event) {
	switch (event->type) {
	case R_EVENT_MOUSE_MOVE: {
		Rid owner = owner_of(event->target);
		g_x = event->x;
		g_y = event->y;
		if (owner == g_hovered) return R_FALSE; /* a shown tooltip stays where it appeared */
		hide();
		g_hovered = owner;
		g_suppressed = 0;
		if (owner == R_INVALID_ID) return R_FALSE;
		if (Rget_time() - g_hidden_at < TIP_QUICK) show();
		else g_timer = Rtimer_create(TIP_DELAY, R_FALSE, on_timer, NULL);
		return R_FALSE;
	}
	case R_EVENT_MOUSE_DOWN:
	case R_EVENT_MOUSE_WHEEL:
	case R_EVENT_KEY_DOWN:
		/* using the widget dismisses its tooltip until the pointer goes elsewhere */
		hide();
		g_hidden_at = -10.0;
		g_suppressed = 1;
		return R_FALSE;
	case R_EVENT_MOUSE_LEAVE:
		hide();
		g_hovered = R_INVALID_ID;
		return R_FALSE;
	default:
		return R_FALSE;
	}
}

void tooltip_init(void) {
	app_add_event_hook(tooltip_hook);
}

/* After the window is built, and after each live reload. */
void tooltip_scan(void) {
	/* a reload keeps the widgets it can (their texts are already here) and sets
	 * Tooltip= again on those it changed or built: take those */
	hide();
	g_hovered = R_INVALID_ID;
	scan(Rwindow_get_content(g_window));
}
