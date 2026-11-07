/*
 * Scrolling that stays where it started.
 *
 * Containment (the whole window), as the CSS overscroll-behavior: contain.
 *   In Raven a widget that cannot scroll further (at its top or bottom)
 *   lets the wheel go to what contains it: an editor scrolled to its end
 *   starts scrolling the page. Here, a widget that has scrolled once keeps
 *   the wheel: the ScrollBoxes around it ignore the wheel over it.
 *
 *   How it is known that a widget scrolls: each ScrollBox gets an event
 *   callback, which sees the wheel events that bubble up to it, before it
 *   scrolls. The window hook notes the innermost widget under the pointer
 *   that could scroll; when a wheel event never reached any ScrollBox above
 *   it, that widget took it, so it scrolls. A widget that never took one (a
 *   short list, a picture) still lets the page scroll over it.
 *   Class="scroll-chain" on a widget lets its wheel go up again.
 *
 * No bounce (Class="no-bounce" on a ScrollBox).
 *   At an end, Raven turns the push of the wheel into a spring: the content
 *   goes past the end and comes back. Over such a ScrollBox, a wheel that
 *   pushes against an end it has reached is dropped before the box sees it.
 */

#include <math.h>
#include <string.h>
#include "app.h"

#define MAX_KNOWN 512

static Rid g_known[MAX_KNOWN];
static int g_known_count, g_known_next;

static Rid g_inner = R_INVALID_ID; /* innermost widget that could scroll, under the last wheel */
static int g_inner_seen;           /* a ScrollBox above it saw the event: it did not take it */
static int g_pending;

static int known(Rid widget) {
	int i;
	for (i = 0; i < g_known_count; i++) {
		if (g_known[i] == widget) return R_TRUE;
	}
	return R_FALSE;
}

static void remember(Rid widget) {
	if (widget == R_INVALID_ID || known(widget)) return;
	if (g_known_count < MAX_KNOWN) g_known[g_known_count++] = widget;
	else g_known[g_known_next++ % MAX_KNOWN] = widget;
}

static int may_scroll(Rid widget) {
	switch (Rwidget_get_type(widget)) {
	case R_WIDGET_SCROLL_BOX:
	case R_WIDGET_TEXT_EDITOR:
	case R_WIDGET_LIST_VIEW:
	case R_WIDGET_TREE_VIEW:
	case R_WIDGET_TABLE:
	case R_WIDGET_TILE_VIEW:
	case R_WIDGET_FILE_BROWSER:
	case R_WIDGET_NODE_GRAPH:
	case R_WIDGET_TIMELINE:
	case R_WIDGET_CUSTOM:
		return R_TRUE;
	default:
		return R_FALSE;
	}
}

/* The innermost widget that could scroll, from `widget` up to `stop` (excluded). */
static Rid innermost(Rid widget, Rid stop) {
	for (; widget != R_INVALID_ID && widget != stop; widget = Rwidget_get_parent(widget)) {
		if (may_scroll(widget)) return widget;
	}
	return R_INVALID_ID;
}

static int scroll_box_event(Rid box, const REvent* event, void* user_data) {
	Rid inner;
	const char* classes;
	(void)user_data;
	if (event->type != R_EVENT_MOUSE_WHEEL) return R_FALSE;
	inner = innermost(event->target, box);
	if (inner == R_INVALID_ID) return R_FALSE; /* over the box itself: it scrolls */
	if (inner == g_inner) g_inner_seen = 1;     /* it came up: the inner widget did not take it */
	classes = Rwidget_get_class(inner);
	if (classes && strstr(classes, "scroll-chain")) return R_FALSE;
	/* a widget that scrolls keeps the wheel, even at its ends: this box does not move */
	return known(inner) ? R_TRUE : R_FALSE;
}

/* A ScrollBox with Class="no-bounce" at an end, pushed further: R_TRUE drops the wheel. */
static int against_end(const REvent* event) {
	Rid widget;
	for (widget = event->target; widget != R_INVALID_ID; widget = Rwidget_get_parent(widget)) {
		const char* classes;
		float ox, oy, x, y, w, h, cw, ch, max_y, max_x;
		Rid child;
		if (Rwidget_get_type(widget) != R_WIDGET_SCROLL_BOX) continue;
		classes = Rwidget_get_class(widget);
		if (!classes || !strstr(classes, "no-bounce")) return R_FALSE; /* the nearest box decides */
		child = Rwidget_get_child(widget, 0);
		if (child == R_INVALID_ID) return R_FALSE;
		Rscroll_box_get_offset(widget, &ox, &oy);
		Rwidget_get_rect(widget, &x, &y, &w, &h);
		Rwidget_get_rect(child, &x, &y, &cw, &ch);
		max_y = ch - h > 0.0f ? ch - h : 0.0f;
		max_x = cw - w > 0.0f ? cw - w : 0.0f;
		/* wheel up (positive) at the top, down at the bottom; the same sideways */
		if ((event->wheel_y > 0.0f && oy <= 0.5f) || (event->wheel_y < 0.0f && oy >= max_y - 0.5f)) {
			if (event->wheel_x == 0.0f || (event->wheel_x > 0.0f && ox <= 0.5f) || (event->wheel_x < 0.0f && ox >= max_x - 0.5f)) return R_TRUE;
		}
		return R_FALSE;
	}
	return R_FALSE;
}

static int contain_hook(const REvent* event) {
	if (event->type != R_EVENT_MOUSE_WHEEL) return R_FALSE;
	/* the last wheel: no ScrollBox above its inner widget saw it, so that widget took it */
	if (g_pending && !g_inner_seen) remember(g_inner);
	g_inner = innermost(event->target, R_INVALID_ID);
	g_inner_seen = 0;
	g_pending = g_inner != R_INVALID_ID;
	/* nothing to note for a ScrollBox under the pointer: its own callback runs first */
	if (g_pending && Rwidget_get_type(g_inner) == R_WIDGET_SCROLL_BOX && innermost(Rwidget_get_parent(g_inner), R_INVALID_ID) == R_INVALID_ID) {
		g_pending = 0;
	}
	return against_end(event);
}

static void attach(Rid widget) {
	int i, n;
	if (widget == R_INVALID_ID) return;
	if (Rwidget_get_type(widget) == R_WIDGET_SCROLL_BOX) Rwidget_set_event_callback(widget, scroll_box_event, NULL);
	n = Rwidget_get_child_count(widget);
	for (i = 0; i < n; i++) attach(Rwidget_get_child(widget, i));
}

void scroll_contain_init(void) {
	app_add_event_hook(contain_hook);
}

/* After the window is built, and after each live reload: every ScrollBox gets the callback. */
void scroll_contain_bind(void) {
	attach(Rwindow_get_content(g_window));
}
