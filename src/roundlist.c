/*
 * <RoundList RowRadius="8" RowHeight="32"/>: a list of rows with rounded
 * corners (custom), for the omni bar.
 *
 * Raven's ListView rounds its rows only in the glass looks. This one is
 * painted: a title and a detail per row, the hovered and the selected rows
 * as rounded pills, smooth scrolling. The row under the pointer is found
 * while painting, from the scroll shown at that moment, so the hover never
 * stays behind while the list scrolls.
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app.h"

typedef struct RoundList {
	char** titles;
	char** details;
	int count;
	int selected;
	float row_h, radius;
	float scroll;          /* where the scroll goes */
	RSpring scroll_shown;  /* where it is drawn */
	float pointer_y;
	int inside;
	int pressed;
	float width, height;
	RoundListCallback activate;
	void* activate_user;
} RoundList;

static RoundList* rl_of(Rid widget) {
	return (RoundList*)Rwidget_get_user_handle(widget);
}

static void rl_free_items(RoundList* rl) {
	int i;
	for (i = 0; i < rl->count; i++) {
		free(rl->titles[i]);
		free(rl->details[i]);
	}
	free(rl->titles);
	free(rl->details);
	rl->titles = rl->details = NULL;
	rl->count = 0;
}

static float rl_max_scroll(const RoundList* rl) {
	float max = (float)rl->count * rl->row_h - rl->height;
	return max > 0.0f ? max : 0.0f;
}

static void rl_clamp(RoundList* rl) {
	if (rl->scroll > rl_max_scroll(rl)) rl->scroll = rl_max_scroll(rl);
	if (rl->scroll < 0.0f) rl->scroll = 0.0f;
}

static void rl_measure(Rid widget, float* width, float* height, void* user_data) {
	(void)widget;
	(void)user_data;
	*width = 300.0f;
	*height = 240.0f;
}

static void rl_paint(Rid widget, RPainter* p, float w, float h, void* user_data) {
	RoundList* rl = (RoundList*)user_data;
	float text_r, text_g, text_b, text_a, ar, ag, ab, aa, tr, tg, tb, ta, dummy, text_h, scroll;
	float size = Rtheme_get_metric(R_METRIC_FONT_SIZE);
	int first, i, hot;

	rl->width = w;
	rl->height = h;
	rl_clamp(rl);
	scroll = Rdraw_spring(p, &rl->scroll_shown, rl->scroll, R_MOTION_SMOOTH);
	Rwidget_get_color(widget, R_COLOR_TEXT, &text_r, &text_g, &text_b, &text_a);
	Rwidget_get_color(widget, R_COLOR_ACCENT, &ar, &ag, &ab, &aa);
	Rwidget_get_color(widget, R_COLOR_ACCENT_TEXT, &tr, &tg, &tb, &ta);
	Rfont_measure_text(Rtheme_get_font(), size, "Ag", &dummy, &text_h);

	/* the row under the pointer, with the scroll as it is drawn now */
	hot = rl->inside ? (int)floorf((rl->pointer_y + scroll) / rl->row_h) : -1;
	if (hot >= rl->count) hot = -1;

	Rdraw_push_clip_rect(p, 0.0f, 0.0f, w, h);
	if (rl->count == 0) {
		float tw, th;
		Rfont_measure_text(Rtheme_get_font(), size, "No results", &tw, &th);
		Rdraw_text(p, R_INVALID_ID, size, (w - tw) * 0.5f, 18.0f, "No results", text_r, text_g, text_b, text_a * 0.5f);
	}
	first = (int)floorf(scroll / rl->row_h);
	if (first < 0) first = 0;
	for (i = first; i < rl->count; i++) {
		float y = (float)i * rl->row_h - scroll, ty = y + (rl->row_h - text_h) * 0.5f;
		int selected = i == rl->selected;
		if (y > h) break;
		if (selected) Rdraw_rect(p, 0.0f, y + 1.0f, w, rl->row_h - 2.0f, rl->radius, ar, ag, ab, 1.0f);
		else if (i == hot) Rdraw_rect(p, 0.0f, y + 1.0f, w, rl->row_h - 2.0f, rl->radius, text_r, text_g, text_b, rl->pressed ? 0.14f : 0.08f);
		Rdraw_push_clip_rect(p, 12.0f, y, w - 24.0f, rl->row_h);
		if (selected) {
			Rdraw_text(p, R_INVALID_ID, size, 12.0f, ty, rl->titles[i], tr, tg, tb, ta);
		} else {
			Rdraw_text(p, R_INVALID_ID, size, 12.0f, ty, rl->titles[i], text_r, text_g, text_b, text_a);
		}
		if (rl->details[i][0]) {
			float dw, dh;
			Rfont_measure_text(Rtheme_get_font(), size - 1.0f, rl->details[i], &dw, &dh);
			if (selected) Rdraw_text(p, R_INVALID_ID, size - 1.0f, w - 12.0f - dw, ty + 1.0f, rl->details[i], tr, tg, tb, ta * 0.75f);
			else Rdraw_text(p, R_INVALID_ID, size - 1.0f, w - 12.0f - dw, ty + 1.0f, rl->details[i], text_r, text_g, text_b, text_a * 0.5f);
		}
		Rdraw_pop_clip_rect(p);
	}
	/* a thin scroll bar, when the rows do not fit */
	if (rl_max_scroll(rl) > 0.0f) {
		float total = (float)rl->count * rl->row_h, bar = h * h / total;
		float y = (h - bar) * (scroll / rl_max_scroll(rl));
		Rdraw_rect(p, w - 4.0f, y + 2.0f, 3.0f, bar - 4.0f, 1.5f, text_r, text_g, text_b, 0.25f);
	}
	Rdraw_pop_clip_rect(p);
}

static int rl_row_at(const RoundList* rl, float y) {
	int row = (int)floorf((y + rl->scroll_shown.value) / rl->row_h);
	return row >= 0 && row < rl->count ? row : -1;
}

static int rl_event(Rid widget, const REvent* event, void* user_data) {
	RoundList* rl = (RoundList*)user_data;
	switch (event->type) {
	case R_EVENT_MOUSE_MOVE:
		rl->pointer_y = event->local_y;
		rl->inside = 1;
		Rwidget_invalidate(widget);
		return R_FALSE;
	case R_EVENT_MOUSE_LEAVE:
		rl->inside = 0;
		rl->pressed = 0;
		Rwidget_invalidate(widget);
		return R_FALSE;
	case R_EVENT_MOUSE_WHEEL: {
		float old = rl->scroll;
		rl->scroll -= event->wheel_y * 48.0f;
		rl_clamp(rl);
		Rwidget_invalidate(widget);
		return rl->scroll != old;
	}
	case R_EVENT_MOUSE_DOWN:
		if (event->button != R_MOUSE_LEFT) return R_FALSE;
		rl->pressed = 1;
		if (rl_row_at(rl, event->local_y) >= 0) rl->selected = rl_row_at(rl, event->local_y);
		Rwidget_invalidate(widget);
		return R_TRUE;
	case R_EVENT_MOUSE_UP: {
		int row = rl_row_at(rl, event->local_y);
		if (event->button != R_MOUSE_LEFT || !rl->pressed) return R_FALSE;
		rl->pressed = 0;
		Rwidget_invalidate(widget);
		/* a click runs the row, as in Spotlight */
		if (row >= 0 && row == rl->selected && rl->activate) rl->activate(widget, row, rl->activate_user);
		return R_TRUE;
	}
	default:
		return R_FALSE;
	}
}

static void rl_destroy(Rid widget, void* user_data) {
	RoundList* rl = (RoundList*)user_data;
	(void)widget;
	rl_free_items(rl);
	free(rl);
}

static Rid rl_create(const char* tag, void* user_data) {
	Rid widget = Rwidget_create(R_WIDGET_CUSTOM);
	RoundList* rl = (RoundList*)calloc(1, sizeof(RoundList));
	(void)tag;
	(void)user_data;
	rl->selected = -1;
	rl->row_h = 32.0f;
	rl->radius = 8.0f;
	Rwidget_set_user_handle(widget, rl);
	Rwidget_set_clipping(widget, R_TRUE);
	Rcustom_set_measure_callback(widget, rl_measure, rl);
	Rcustom_set_paint_callback(widget, rl_paint, rl);
	Rwidget_set_event_callback(widget, rl_event, rl);
	Rwidget_set_destroy_callback(widget, rl_destroy, rl);
	return widget;
}

static int rl_attribute(Rid widget, const char* name, const char* value, void* user_data) {
	RoundList* rl = rl_of(widget);
	(void)user_data;
	if (!rl) return R_FALSE;
	if (strcasecmp(name, "RowRadius") == 0) rl->radius = (float)atof(value);
	else if (strcasecmp(name, "RowHeight") == 0) rl->row_h = (float)atof(value) > 12.0f ? (float)atof(value) : 12.0f;
	else return R_FALSE;
	return R_TRUE;
}

void roundlist_register(void) {
	Rxml_register_tag("RoundList", rl_create, rl_attribute, NULL);
}

void roundlist_set_items(Rid widget, int count, const char* const* titles, const char* const* details) {
	RoundList* rl = rl_of(widget);
	int i;
	if (!rl) return;
	rl_free_items(rl);
	rl->titles = (char**)calloc((size_t)(count > 0 ? count : 1), sizeof(char*));
	rl->details = (char**)calloc((size_t)(count > 0 ? count : 1), sizeof(char*));
	for (i = 0; i < count; i++) {
		rl->titles[i] = strdup(titles[i] ? titles[i] : "");
		rl->details[i] = strdup(details && details[i] ? details[i] : "");
	}
	rl->count = count;
	rl->scroll = 0.0f;
	Rspring_set(&rl->scroll_shown, 0.0f);
	if (rl->selected >= count) rl->selected = count - 1;
	Rwidget_invalidate(widget);
}

int roundlist_get_selected(Rid widget) {
	RoundList* rl = rl_of(widget);
	return rl ? rl->selected : -1;
}

/* Selects a row and scrolls so that it shows. */
void roundlist_set_selected(Rid widget, int index) {
	RoundList* rl = rl_of(widget);
	float top;
	if (!rl) return;
	rl->selected = index >= 0 && index < rl->count ? index : -1;
	if (rl->selected >= 0 && rl->height > 0.0f) {
		top = (float)rl->selected * rl->row_h;
		if (top < rl->scroll) rl->scroll = top;
		else if (top + rl->row_h > rl->scroll + rl->height) rl->scroll = top + rl->row_h - rl->height;
		rl_clamp(rl);
	}
	Rwidget_invalidate(widget);
}

void roundlist_set_activate_callback(Rid widget, RoundListCallback callback, void* user_data) {
	RoundList* rl = rl_of(widget);
	if (!rl) return;
	rl->activate = callback;
	rl->activate_user = user_data;
}
