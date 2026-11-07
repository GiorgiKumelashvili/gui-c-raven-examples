/*
 * <SegmentedControl Items="List|Grid|Gallery" Selected="0" Intent="Primary"
 *                   Large="false" OnChanged="action"/>
 *
 * Not a widget of Raven: a CUSTOM widget, painted with Rdraw_ and taught to
 * the XML loader with Rxml_register_tag. The thumb slides to the chosen
 * segment on a spring (Rdraw_spring), so it overshoots a little and settles,
 * as the rest of Raven moves. Left / Right change the choice from the keyboard.
 */

#include <stdlib.h>
#include <string.h>
#include "app.h"

#define SEG_MAX 8

typedef struct Segmented {
	char* items[SEG_MAX];
	int count;
	int selected;
	int hovered;
	int primary;
	int large;
	float width;
	RSpring thumb_x;
	RSpring thumb_w;
} Segmented;

static Segmented* seg_of(Rid widget) {
	return (Segmented*)Rwidget_get_user_handle(widget);
}

static void seg_set_items(Segmented* seg, const char* text) {
	int i;
	const char* start = text;
	for (i = 0; i < seg->count; i++) free(seg->items[i]);
	seg->count = 0;
	while (start && *start && seg->count < SEG_MAX) {
		const char* bar = strchr(start, '|');
		size_t length = bar ? (size_t)(bar - start) : strlen(start);
		char* item = (char*)malloc(length + 1);
		memcpy(item, start, length);
		item[length] = '\0';
		seg->items[seg->count++] = item;
		start = bar ? bar + 1 : NULL;
	}
	if (seg->selected >= seg->count) seg->selected = seg->count - 1;
}

static float seg_font_size(const Segmented* seg) {
	return Rtheme_get_metric(R_METRIC_FONT_SIZE) + (seg->large ? 1.0f : 0.0f);
}

static float seg_height(const Segmented* seg) {
	float height = Rtheme_get_metric(R_METRIC_CONTROL_HEIGHT);
	if (height < 26.0f) height = 26.0f;
	return seg->large ? height + 8.0f : height;
}

static void seg_measure(Rid widget, float* width, float* height, void* user_data) {
	Segmented* seg = seg_of(widget);
	float widest = 0.0f;
	int i;
	(void)user_data;
	for (i = 0; i < seg->count; i++) {
		float w = 0.0f, h = 0.0f;
		Rfont_measure_text(Rtheme_get_font(), seg_font_size(seg), seg->items[i], &w, &h);
		if (w > widest) widest = w;
	}
	*width = (widest + (seg->large ? 36.0f : 28.0f)) * (float)(seg->count > 0 ? seg->count : 1) + 6.0f;
	*height = seg_height(seg);
}

static void seg_paint(Rid widget, RPainter* painter, float width, float height, void* user_data) {
	Segmented* seg = seg_of(widget);
	float r, g, b, a, tr, tg, tb, ta, ar, ag, ab, aa, cr, cg, cb, ca;
	float inset = 3.0f, segment, radius, x, w, font_size, text_h, dummy;
	int i;
	(void)user_data;

	seg->width = width;
	if (seg->count == 0) return;
	segment = (width - inset * 2.0f) / (float)seg->count;
	radius = Rtheme_get_metric(R_METRIC_CORNER_RADIUS) + 2.0f;
	font_size = seg_font_size(seg);
	Rfont_measure_text(Rtheme_get_font(), font_size, "Ag", &dummy, &text_h);

	/* the track */
	Rwidget_get_color(widget, R_COLOR_TRACK, &r, &g, &b, &a);
	Rdraw_rect(painter, 0.0f, 0.0f, width, height, radius, r, g, b, a);

	/* the hovered segment */
	if (seg->hovered >= 0 && seg->hovered != seg->selected) {
		Rwidget_get_color(widget, R_COLOR_CONTROL_HOVERED, &r, &g, &b, &a);
		Rdraw_rect(painter, inset + segment * (float)seg->hovered, inset, segment, height - inset * 2.0f, radius - 1.0f, r, g, b, a * 0.6f);
	}

	/* the thumb, on its springs */
	if (seg->selected >= 0) {
		x = Rdraw_spring(painter, &seg->thumb_x, inset + segment * (float)seg->selected, R_MOTION_BOUNCY);
		w = Rdraw_spring(painter, &seg->thumb_w, segment, R_MOTION_SMOOTH);
		Rwidget_get_color(widget, R_COLOR_SHADOW, &r, &g, &b, &a);
		Rdraw_rect(painter, x, inset + 1.0f, w, height - inset * 2.0f, radius - 1.0f, r, g, b, a * 0.35f);
		if (seg->primary) Rwidget_get_color(widget, R_COLOR_ACCENT, &cr, &cg, &cb, &ca);
		else Rwidget_get_color(widget, R_COLOR_CONTROL, &cr, &cg, &cb, &ca);
		Rdraw_rect(painter, x, inset, w, height - inset * 2.0f, radius - 1.0f, cr, cg, cb, 1.0f);
		Rwidget_get_color(widget, R_COLOR_CONTROL_BORDER, &r, &g, &b, &a);
		if (!seg->primary) Rdraw_rect_outline(painter, x, inset, w, height - inset * 2.0f, radius - 1.0f, 1.0f, r, g, b, a * 0.6f);
	}

	/* the separators between unselected segments */
	Rwidget_get_color(widget, R_COLOR_SEPARATOR, &r, &g, &b, &a);
	for (i = 1; i < seg->count; i++) {
		if (i == seg->selected || i - 1 == seg->selected) continue;
		x = inset + segment * (float)i;
		Rdraw_line(painter, x, height * 0.3f, x, height * 0.7f, 1.0f, r, g, b, a);
	}

	/* the labels */
	Rwidget_get_color(widget, R_COLOR_TEXT, &tr, &tg, &tb, &ta);
	Rwidget_get_color(widget, R_COLOR_ACCENT_TEXT, &ar, &ag, &ab, &aa);
	for (i = 0; i < seg->count; i++) {
		float text_w = 0.0f, text_hh = 0.0f, tx, ty;
		int on = i == seg->selected;
		Rfont_measure_text(Rtheme_get_font(), font_size, seg->items[i], &text_w, &text_hh);
		tx = inset + segment * (float)i + (segment - text_w) * 0.5f;
		ty = (height - text_h) * 0.5f;
		if (on && seg->primary) Rdraw_text(painter, R_INVALID_ID, font_size, tx, ty, seg->items[i], ar, ag, ab, aa);
		else Rdraw_text(painter, R_INVALID_ID, font_size, tx, ty, seg->items[i], tr, tg, tb, on ? ta : ta * 0.72f);
	}

	/* keyboard focus */
	if (Rwidget_is_focused(widget)) {
		Rwidget_get_color(widget, R_COLOR_FOCUS, &r, &g, &b, &a);
		Rdraw_rect_outline(painter, 0.0f, 0.0f, width, height, radius, 2.0f, r, g, b, a);
	}
}

static int seg_index_at(const Segmented* seg, float x) {
	int index;
	if (seg->count == 0 || seg->width <= 0.0f) return -1;
	index = (int)((x - 3.0f) / ((seg->width - 6.0f) / (float)seg->count));
	if (index < 0) index = 0;
	if (index >= seg->count) index = seg->count - 1;
	return index;
}

static void seg_choose(Rid widget, Segmented* seg, int index) {
	const char* action;
	if (index < 0 || index >= seg->count || index == seg->selected) return;
	seg->selected = index;
	Rwidget_invalidate(widget);
	action = Rwidget_get_action(widget);
	if (action && action[0]) Raction_fire(action, widget);
}

static int seg_event(Rid widget, const REvent* event, void* user_data) {
	Segmented* seg = seg_of(widget);
	int index;
	(void)user_data;
	if (!seg) return R_FALSE;
	switch (event->type) {
	case R_EVENT_MOUSE_MOVE:
		index = seg_index_at(seg, event->local_x);
		if (index != seg->hovered) {
			seg->hovered = index;
			Rwidget_invalidate(widget);
		}
		return R_FALSE;
	case R_EVENT_MOUSE_LEAVE:
		seg->hovered = -1;
		Rwidget_invalidate(widget);
		return R_FALSE;
	case R_EVENT_MOUSE_DOWN:
		if (event->button != R_MOUSE_LEFT) return R_FALSE;
		Rwidget_focus(widget);
		seg_choose(widget, seg, seg_index_at(seg, event->local_x));
		return R_TRUE;
	case R_EVENT_KEY_DOWN:
		if (event->key == R_KEY_LEFT) seg_choose(widget, seg, seg->selected - 1);
		else if (event->key == R_KEY_RIGHT) seg_choose(widget, seg, seg->selected + 1);
		else return R_FALSE;
		return R_TRUE;
	case R_EVENT_FOCUS_GAINED:
	case R_EVENT_FOCUS_LOST:
		Rwidget_invalidate(widget);
		return R_FALSE;
	default:
		return R_FALSE;
	}
}

static void seg_destroy(Rid widget, void* user_data) {
	Segmented* seg = (Segmented*)user_data;
	int i;
	(void)widget;
	for (i = 0; i < seg->count; i++) free(seg->items[i]);
	free(seg);
}

static Rid seg_create(const char* tag, void* user_data) {
	Rid widget = Rwidget_create(R_WIDGET_CUSTOM);
	Segmented* seg = (Segmented*)calloc(1, sizeof(Segmented));
	(void)tag;
	(void)user_data;
	seg->hovered = -1;
	seg_set_items(seg, "One|Two|Three");
	Rwidget_set_user_handle(widget, seg);
	Rwidget_set_focusable(widget, R_TRUE);
	Rwidget_set_cursor(widget, R_CURSOR_HAND);
	Rcustom_set_measure_callback(widget, seg_measure, NULL);
	Rcustom_set_paint_callback(widget, seg_paint, NULL);
	Rwidget_set_event_callback(widget, seg_event, NULL);
	Rwidget_set_destroy_callback(widget, seg_destroy, seg);
	return widget;
}

static int same(const char* a, const char* b) {
	for (; *a && *b; a++, b++) {
		char ca = (char)(*a >= 'A' && *a <= 'Z' ? *a + 32 : *a);
		char cb = (char)(*b >= 'A' && *b <= 'Z' ? *b + 32 : *b);
		if (ca != cb) return 0;
	}
	return *a == *b;
}

static int is_true(const char* value) {
	return same(value, "true") || same(value, "1") || same(value, "yes");
}

static int seg_attribute(Rid widget, const char* name, const char* value, void* user_data) {
	Segmented* seg = seg_of(widget);
	(void)user_data;
	if (!seg) return R_FALSE;
	if (same(name, "Items")) seg_set_items(seg, value);
	else if (same(name, "Selected")) seg->selected = atoi(value);
	else if (same(name, "Intent")) seg->primary = same(value, "Primary");
	else if (same(name, "Large")) seg->large = is_true(value);
	else return R_FALSE;
	Rwidget_invalidate(widget);
	return R_TRUE;
}

int segmented_get_selected(Rid widget) {
	Segmented* seg = seg_of(widget);
	return seg ? seg->selected : -1;
}

const char* segmented_get_item(Rid widget, int index) {
	Segmented* seg = seg_of(widget);
	return seg && index >= 0 && index < seg->count ? seg->items[index] : "";
}

void segmented_register(void) {
	Rxml_register_tag("SegmentedControl", seg_create, seg_attribute, NULL);
}
