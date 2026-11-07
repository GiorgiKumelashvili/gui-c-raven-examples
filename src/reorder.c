/*
 * Rows of Raven's ListView and Table moved by the user.
 *
 * Neither widget can reorder its rows, and neither tells which row is under
 * a point. So:
 *   - a press on a row lets the widget select it: that is the row dragged;
 *   - once the pointer moved, the window hook takes the moves and shows a
 *     line where the row would land (the Border "drop_line" of main.xml);
 *   - on release, a click is injected where the pointer is (on the next
 *     frame, outside of the event being handled): the widget selects the row
 *     under it, which is the target. The texts are then moved and rewritten.
 * Alt+Up and Alt+Down move the selected row from the keyboard.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

typedef enum { KIND_LIST, KIND_TABLE } Kind;

typedef struct Target {
	const char* name;
	Kind kind;
} Target;

static const Target k_targets[] = { { "reorder_list", KIND_LIST }, { "raven_table", KIND_TABLE } };
#define TARGET_COUNT ((int)(sizeof(k_targets) / sizeof(k_targets[0])))

static struct {
	const Target* target;
	Rid widget;
	int pressed;
	int dragging;
	int from;
	float start_x, start_y;
	int drop_pending;
	float drop_x, drop_y;
} g;

static float row_height(void) {
	return Rtheme_get_metric(R_METRIC_ROW_HEIGHT);
}

/* Where the rows start inside the widget (the header of a table is a row and a little more). */
static float body_top(const Target* target, Rid widget) {
	float x, y, w, h;
	Rwidget_get_rect(widget, &x, &y, &w, &h);
	return y + 1.0f + (target->kind == KIND_TABLE ? row_height() + 4.0f : 0.0f);
}

static int selected(const Target* target, Rid widget) {
	return target->kind == KIND_LIST ? Rlist_view_get_selected(widget) : Rtable_get_selected(widget);
}

static int count(const Target* target, Rid widget) {
	return target->kind == KIND_LIST ? Rlist_view_get_item_count(widget) : Rtable_get_row_count(widget);
}

/* Moves row `from` to `to`, by rewriting the texts (neither widget can insert). */
static void move_row(const Target* target, Rid widget, int from, int to) {
	int n = count(target, widget), columns, i, c, step;
	char** texts;
	if (from < 0 || to < 0 || from >= n || to >= n || from == to) return;
	columns = target->kind == KIND_LIST ? 1 : Rtable_get_column_count(widget);
	texts = (char**)calloc((size_t)(n * columns), sizeof(char*));
	if (target->kind == KIND_TABLE) Rtable_sort(widget, -1, 0); /* keeps the order shown, drops the sort arrow */
	for (i = 0; i < n; i++) {
		for (c = 0; c < columns; c++) {
			const char* text = target->kind == KIND_LIST ? Rlist_view_get_item_text(widget, i) : Rtable_get_cell(widget, i, c);
			texts[i * columns + c] = strdup(text ? text : "");
		}
	}
	/* rotate the rows between from and to */
	step = to > from ? 1 : -1;
	for (i = from; i != to; i += step) {
		for (c = 0; c < columns; c++) {
			char* swap = texts[i * columns + c];
			texts[i * columns + c] = texts[(i + step) * columns + c];
			texts[(i + step) * columns + c] = swap;
		}
	}
	for (i = 0; i < n; i++) {
		for (c = 0; c < columns; c++) {
			if (target->kind == KIND_LIST) Rlist_view_set_item_text(widget, i, texts[i * columns + c]);
			else Rtable_set_cell(widget, i, c, texts[i * columns + c]);
			free(texts[i * columns + c]);
		}
	}
	free(texts);
	if (target->kind == KIND_LIST) Rlist_view_set_selected(widget, to);
	else Rtable_set_selected(widget, to);
}

static const Target* target_at(float x, float y, Rid* widget) {
	int i;
	for (i = 0; i < TARGET_COUNT; i++) {
		Rid w = Rwidget_find(g_window, k_targets[i].name);
		float rx, ry, rw, rh;
		if (w == R_INVALID_ID || Rwidget_get_visibility(w) != R_VISIBLE) continue;
		Rwidget_get_rect(w, &rx, &ry, &rw, &rh);
		if (rw <= 0.0f || x < rx || y < body_top(&k_targets[i], w) || x >= rx + rw - 14.0f || y >= ry + rh) continue;
		*widget = w;
		return &k_targets[i];
	}
	return NULL;
}

static const Target* focused_target(Rid* widget) {
	int i;
	for (i = 0; i < TARGET_COUNT; i++) {
		Rid w = Rwidget_find(g_window, k_targets[i].name);
		if (w != R_INVALID_ID && Rwidget_is_focused(w)) {
			*widget = w;
			return &k_targets[i];
		}
	}
	return NULL;
}

static void show_line(float y) {
	Rid line = Rwidget_find(g_window, "drop_line");
	Rid layer = Rwidget_find(g_window, "popover_layer");
	float wx, wy, ww, wh, lx, ly, lw, lh, top, snapped;
	int n, index;
	if (line == R_INVALID_ID || layer == R_INVALID_ID) return;
	Rwidget_get_rect(g.widget, &wx, &wy, &ww, &wh);
	Rwidget_get_rect(layer, &lx, &ly, &lw, &lh);
	/* the boundary nearest to the pointer, counting from the top of the rows (unscrolled) */
	top = body_top(g.target, g.widget);
	n = count(g.target, g.widget);
	index = (int)floorf((y - top) / row_height() + 0.5f);
	if (index < 0) index = 0;
	if (index > n) index = n;
	snapped = top + (float)index * row_height();
	if (snapped > wy + wh - 2.0f) snapped = wy + wh - 2.0f;
	Rslot_set_anchors(line, 0.0f, 0.0f, 0.0f, 0.0f);
	Rslot_set_position(line, wx - lx + 4.0f, snapped - ly - 1.0f);
	Rslot_set_size(line, ww - 8.0f, 3.0f);
	Rwidget_set_visibility(line, R_VISIBLE);
}

static void hide_line(void) {
	Rid line = Rwidget_find(g_window, "drop_line");
	if (line != R_INVALID_ID) Rwidget_set_visibility(line, R_COLLAPSED);
}

static int reorder_hook(const REvent* event) {
	Rid widget = R_INVALID_ID;
	const Target* target;
	switch (event->type) {
	case R_EVENT_MOUSE_DOWN:
		g.pressed = g.dragging = 0;
		if (event->button != R_MOUSE_LEFT) return R_FALSE;
		target = target_at(event->x, event->y, &widget);
		if (!target) return R_FALSE;
		g.target = target;
		g.widget = widget;
		g.pressed = 1;
		g.start_x = event->x;
		g.start_y = event->y;
		return R_FALSE; /* the widget selects the row pressed */
	case R_EVENT_MOUSE_MOVE:
		if (!g.pressed) return R_FALSE;
		if (!g.dragging) {
			if (fabsf(event->y - g.start_y) < 6.0f) return R_FALSE;
			g.from = selected(g.target, g.widget);
			if (g.from < 0) {
				g.pressed = 0;
				return R_FALSE;
			}
			g.dragging = 1;
			Rwidget_set_cursor(g.widget, R_CURSOR_MOVE);
		}
		show_line(event->y);
		return R_TRUE;
	case R_EVENT_MOUSE_UP:
		if (!g.pressed) return R_FALSE;
		g.pressed = 0;
		if (!g.dragging) return R_FALSE;
		g.dragging = 0;
		hide_line();
		Rwidget_set_cursor(g.widget, R_CURSOR_ARROW);
		g.drop_pending = 1;
		g.drop_x = event->x;
		g.drop_y = event->y;
		Rwake_up();
		return R_TRUE;
	case R_EVENT_KEY_DOWN:
		if (!(event->modifiers & R_MOD_ALT) || (event->key != R_KEY_UP && event->key != R_KEY_DOWN)) return R_FALSE;
		target = focused_target(&widget);
		if (!target) return R_FALSE;
		{
			int from = selected(target, widget);
			move_row(target, widget, from, from + (event->key == R_KEY_UP ? -1 : 1));
		}
		return R_TRUE;
	default:
		return R_FALSE;
	}
}

/* Every frame: a drop waiting for its target row. */
void reorder_frame(void) {
	float rx, ry, rw, rh, y;
	int to;
	if (!g.drop_pending) return;
	g.drop_pending = 0;
	Rwidget_get_rect(g.widget, &rx, &ry, &rw, &rh);
	/* above or below the rows: the first or the last */
	y = g.drop_y;
	if (y < body_top(g.target, g.widget)) y = body_top(g.target, g.widget) + 2.0f;
	if (y > ry + rh - 3.0f) y = ry + rh - 3.0f;
	Rwindow_inject_mouse_move(g_window, g.drop_x, y);
	Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_TRUE);
	Rwindow_inject_mouse_button(g_window, R_MOUSE_LEFT, R_FALSE);
	g.pressed = 0; /* the injected click is not a drag */
	to = selected(g.target, g.widget);
	if (to < 0) to = count(g.target, g.widget) - 1;
	move_row(g.target, g.widget, g.from, to);
}

void reorder_init(void) {
	app_add_event_hook(reorder_hook);
}
