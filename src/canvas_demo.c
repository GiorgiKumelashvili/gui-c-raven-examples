/*
 * Canvas: drawing with Raven, as with an HTML <canvas>.
 *
 * Raven has no <canvas> element; its CUSTOM widget is one: a paint callback
 * receives a painter and draws with Rdraw_rect, _circle, _line, _curve,
 * _triangle, _text, _image, _glass, clipped, with springs for motion. Two
 * tags are made of it here:
 *
 *   <CanvasShowcase/>  every primitive, animated, following the pointer
 *   <SketchPad/>       a surface to draw on with the mouse or a pen
 */

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

/* ---------------------------------------------------------------------------
 *  Showcase
 * ------------------------------------------------------------------------- */

typedef struct Showcase {
	float mouse_x, mouse_y;
	int inside;
	RSpring follow_x, follow_y, pulse;
} Showcase;

static void showcase_measure(Rid widget, float* width, float* height, void* user_data) {
	(void)widget;
	(void)user_data;
	*width = 400.0f;
	*height = 320.0f;
}

static void showcase_paint(Rid widget, RPainter* p, float w, float h, void* user_data) {
	Showcase* s = (Showcase*)user_data;
	float t = (float)Rget_time(), x, y, cx, cy, r;
	int i;

	/* the widget's own drawing is not clipped to it (Clip is for its children): clip it */
	Rdraw_push_clip_rect(p, 0.0f, 0.0f, w, h);
	/* a night sky: bands of color */
	for (i = 0; i < 12; i++) {
		float k = (float)i / 11.0f;
		Rdraw_rect(p, 0.0f, h * (float)i / 12.0f, w, h / 12.0f + 1.0f, 0.0f, 0.06f + 0.10f * k, 0.07f + 0.04f * k, 0.18f + 0.12f * k, 1.0f);
	}
	/* a grid */
	for (x = 0.0f; x < w; x += 32.0f) Rdraw_line(p, x, 0.0f, x, h, 1.0f, 1.0f, 1.0f, 1.0f, 0.04f);
	for (y = 0.0f; y < h; y += 32.0f) Rdraw_line(p, 0.0f, y, w, y, 1.0f, 1.0f, 1.0f, 1.0f, 0.04f);

	/* a wave of bezier curves */
	for (i = 0; i < 4; i++) {
		float base = h * 0.72f + (float)i * 10.0f, phase = t * 1.2f + (float)i * 0.7f;
		Rdraw_curve(p, 0.0f, base, w * 0.3f, base - 50.0f * sinf(phase), w * 0.7f, base + 50.0f * cosf(phase), w, base,
			3.0f - (float)i * 0.5f, 0.39f, 0.82f, 1.0f, 0.8f - (float)i * 0.15f);
	}

	/* a clock: a ring of ticks, hands that turn */
	cx = w * 0.22f;
	cy = h * 0.36f;
	r = 62.0f;
	Rdraw_circle(p, cx, cy, r + 6.0f, 1.0f, 1.0f, 1.0f, 0.08f);
	for (i = 0; i < 60; i++) {
		float a = (float)i / 60.0f * 6.2831853f, inner = i % 5 == 0 ? r - 12.0f : r - 5.0f;
		Rdraw_line(p, cx + cosf(a) * inner, cy + sinf(a) * inner, cx + cosf(a) * r, cy + sinf(a) * r, i % 5 == 0 ? 2.0f : 1.0f, 1.0f, 1.0f, 1.0f, 0.7f);
	}
	Rdraw_line(p, cx, cy, cx + cosf(t * 0.2f - 1.57f) * (r - 26.0f), cy + sinf(t * 0.2f - 1.57f) * (r - 26.0f), 4.0f, 1.0f, 1.0f, 1.0f, 0.95f);
	Rdraw_line(p, cx, cy, cx + cosf(t * 2.0f - 1.57f) * (r - 12.0f), cy + sinf(t * 2.0f - 1.57f) * (r - 12.0f), 2.0f, 1.0f, 0.62f, 0.04f, 1.0f);
	Rdraw_circle(p, cx, cy, 4.0f, 1.0f, 0.62f, 0.04f, 1.0f);

	/* orbiting dots */
	for (i = 0; i < 8; i++) {
		float a = t * 0.9f + (float)i * 0.785f;
		Rdraw_circle(p, w * 0.55f + cosf(a) * 70.0f, h * 0.36f + sinf(a) * 40.0f, 5.0f + 3.0f * (float)(i % 3),
			0.5f + 0.5f * sinf((float)i), 0.4f, 0.9f, 0.9f);
	}

	/* triangles that turn */
	for (i = 0; i < 3; i++) {
		float a = t * (0.6f + 0.3f * (float)i), tx = w * 0.85f, ty = h * 0.22f + (float)i * 52.0f, s2 = 18.0f;
		Rdraw_triangle(p, tx + cosf(a) * s2, ty + sinf(a) * s2, tx + cosf(a + 2.094f) * s2, ty + sinf(a + 2.094f) * s2,
			tx + cosf(a + 4.188f) * s2, ty + sinf(a + 4.188f) * s2, 0.98f - 0.2f * (float)i, 0.35f + 0.2f * (float)i, 0.42f, 1.0f);
	}

	/* rounded rectangles and outlines */
	Rdraw_rect(p, 16.0f, h - 48.0f, 120.0f, 32.0f, 10.0f, 0.24f, 0.39f, 0.87f, 1.0f);
	Rdraw_text(p, R_INVALID_ID, 13.0f, 30.0f, h - 40.0f, "Rdraw_rect", 1.0f, 1.0f, 1.0f, 1.0f);
	Rdraw_rect_outline(p, 146.0f, h - 48.0f, 140.0f, 32.0f, 10.0f, 2.0f, 0.19f, 0.82f, 0.35f, 1.0f);
	Rdraw_text(p, R_INVALID_ID, 13.0f, 160.0f, h - 40.0f, "Rdraw_rect_outline", 0.6f, 0.95f, 0.7f, 1.0f);

	/* a disc that follows the pointer on springs, and pulses when it arrives */
	{
		float tx = s->inside ? s->mouse_x : w * 0.55f, ty = s->inside ? s->mouse_y : h * 0.36f;
		float fx = Rdraw_spring(p, &s->follow_x, tx, R_MOTION_WOBBLY);
		float fy = Rdraw_spring(p, &s->follow_y, ty, R_MOTION_WOBBLY);
		float pulse = Rdraw_spring(p, &s->pulse, s->inside ? 1.0f : 0.0f, R_MOTION_BOUNCY);
		Rdraw_circle(p, fx, fy, 14.0f + 10.0f * pulse, 1.0f, 0.27f, 0.35f, 0.25f);
		Rdraw_circle(p, fx, fy, 9.0f + 3.0f * pulse, 1.0f, 0.27f, 0.35f, 1.0f);
	}

	Rdraw_text(p, R_INVALID_ID, 12.0f, 12.0f, 10.0f, "CUSTOM widget  -  move the pointer over it", 1.0f, 1.0f, 1.0f, 0.6f);
	Rdraw_pop_clip_rect(p);
	/* keep drawing: an animation runs while the widget is shown */
	Rwidget_invalidate(widget);
}

static int showcase_event(Rid widget, const REvent* event, void* user_data) {
	Showcase* s = (Showcase*)user_data;
	(void)widget;
	if (event->type == R_EVENT_MOUSE_MOVE) {
		s->mouse_x = event->local_x;
		s->mouse_y = event->local_y;
		s->inside = 1;
	} else if (event->type == R_EVENT_MOUSE_LEAVE) {
		s->inside = 0;
	}
	return R_FALSE;
}

static void free_user(Rid widget, void* user_data) {
	(void)widget;
	free(user_data);
}

static Rid showcase_create(const char* tag, void* user_data) {
	Rid widget = Rwidget_create(R_WIDGET_CUSTOM);
	Showcase* s = (Showcase*)calloc(1, sizeof(Showcase));
	(void)tag;
	(void)user_data;
	Rwidget_set_clipping(widget, R_TRUE);
	Rcustom_set_measure_callback(widget, showcase_measure, s);
	Rcustom_set_paint_callback(widget, showcase_paint, s);
	Rwidget_set_event_callback(widget, showcase_event, s);
	Rwidget_set_destroy_callback(widget, free_user, s);
	return widget;
}

/* ---------------------------------------------------------------------------
 *  Sketch pad
 * ------------------------------------------------------------------------- */

#define SKETCH_MAX_POINTS 40000
#define SKETCH_MAX_STROKES 2000

typedef struct Stroke {
	int first, count;
	float r, g, b, width;
} Stroke;

typedef struct Sketch {
	float* xy;
	int points;
	Stroke strokes[SKETCH_MAX_STROKES];
	int stroke_count;
	int drawing;
	float r, g, b, width;
} Sketch;

static Rid g_sketch = R_INVALID_ID;

static void sketch_measure(Rid widget, float* width, float* height, void* user_data) {
	(void)widget;
	(void)user_data;
	*width = 300.0f;
	*height = 300.0f;
}

static void sketch_paint(Rid widget, RPainter* p, float w, float h, void* user_data) {
	Sketch* s = (Sketch*)user_data;
	int i, j;
	(void)widget;
	Rdraw_push_clip_rect(p, 0.0f, 0.0f, w, h);
	Rdraw_rect(p, 0.0f, 0.0f, w, h, 10.0f, 0.98f, 0.97f, 0.94f, 1.0f);
	for (i = 1; i * 24.0f < h; i++) Rdraw_line(p, 0.0f, (float)i * 24.0f, w, (float)i * 24.0f, 1.0f, 0.2f, 0.4f, 0.9f, 0.10f);
	for (i = 0; i < s->stroke_count; i++) {
		const Stroke* st = &s->strokes[i];
		const float* pt = s->xy + st->first * 2;
		if (st->count == 1) Rdraw_circle(p, pt[0], pt[1], st->width * 0.5f, st->r, st->g, st->b, 1.0f);
		for (j = 1; j < st->count; j++) {
			Rdraw_line(p, pt[(j - 1) * 2], pt[(j - 1) * 2 + 1], pt[j * 2], pt[j * 2 + 1], st->width, st->r, st->g, st->b, 1.0f);
		}
	}
	if (s->stroke_count == 0) Rdraw_text(p, R_INVALID_ID, 14.0f, 16.0f, 12.0f, "Draw here with the mouse or a pen", 0.2f, 0.2f, 0.25f, 0.45f);
	Rdraw_pop_clip_rect(p);
}

static void sketch_add(Sketch* s, float x, float y) {
	Stroke* st;
	if (s->stroke_count == 0 || s->points >= SKETCH_MAX_POINTS) return;
	st = &s->strokes[s->stroke_count - 1];
	if (st->count > 0) {
		float dx = x - s->xy[(st->first + st->count - 1) * 2], dy = y - s->xy[(st->first + st->count - 1) * 2 + 1];
		if (dx * dx + dy * dy < 2.0f) return; /* closer than a unit and a half: nothing new */
	}
	s->xy[s->points * 2] = x;
	s->xy[s->points * 2 + 1] = y;
	s->points++;
	st->count++;
}

static int sketch_event(Rid widget, const REvent* event, void* user_data) {
	Sketch* s = (Sketch*)user_data;
	switch (event->type) {
	case R_EVENT_MOUSE_DOWN:
		if (event->button != R_MOUSE_LEFT || s->stroke_count >= SKETCH_MAX_STROKES) return R_FALSE;
		s->strokes[s->stroke_count].first = s->points;
		s->strokes[s->stroke_count].count = 0;
		s->strokes[s->stroke_count].r = s->r;
		s->strokes[s->stroke_count].g = s->g;
		s->strokes[s->stroke_count].b = s->b;
		s->strokes[s->stroke_count].width = s->width;
		s->stroke_count++;
		s->drawing = 1;
		sketch_add(s, event->local_x, event->local_y);
		Rwidget_invalidate(widget);
		return R_TRUE;
	case R_EVENT_MOUSE_MOVE:
		if (!s->drawing) return R_FALSE;
		if (!Rwindow_is_mouse_button_down(g_window, R_MOUSE_LEFT)) {
			s->drawing = 0;
			return R_FALSE;
		}
		sketch_add(s, event->local_x, event->local_y);
		Rwidget_invalidate(widget);
		return R_TRUE;
	case R_EVENT_MOUSE_UP:
	case R_EVENT_MOUSE_LEAVE:
		s->drawing = 0;
		return R_FALSE;
	default:
		return R_FALSE;
	}
}

static void sketch_destroy(Rid widget, void* user_data) {
	Sketch* s = (Sketch*)user_data;
	if (g_sketch == widget) g_sketch = R_INVALID_ID;
	free(s->xy);
	free(s);
}

static Rid sketch_create(const char* tag, void* user_data) {
	Rid widget = Rwidget_create(R_WIDGET_CUSTOM);
	Sketch* s = (Sketch*)calloc(1, sizeof(Sketch));
	(void)tag;
	(void)user_data;
	s->xy = (float*)malloc(sizeof(float) * 2 * SKETCH_MAX_POINTS);
	s->r = 0.12f;
	s->g = 0.12f;
	s->b = 0.16f;
	s->width = 3.0f;
	Rwidget_set_user_handle(widget, s);
	Rwidget_set_cursor(widget, R_CURSOR_CROSSHAIR);
	Rwidget_set_clipping(widget, R_TRUE);
	Rwidget_set_attribute(widget, "Touch.Mode", "Mouse"); /* a finger draws, it does not scroll the page */
	Rcustom_set_measure_callback(widget, sketch_measure, s);
	Rcustom_set_paint_callback(widget, sketch_paint, s);
	Rwidget_set_event_callback(widget, sketch_event, s);
	Rwidget_set_destroy_callback(widget, sketch_destroy, s);
	g_sketch = widget;
	return widget;
}

static float hex_digit(char c) {
	if (c >= '0' && c <= '9') return (float)(c - '0');
	if (c >= 'a' && c <= 'f') return (float)(c - 'a' + 10);
	if (c >= 'A' && c <= 'F') return (float)(c - 'A' + 10);
	return 0.0f;
}

/* "sketch.color:#RRGGBB", "sketch.width:N", "sketch.clear", "sketch.undo" */
static void on_sketch(Rid sender, const char* action, void* user_data) {
	Sketch* s = g_sketch != R_INVALID_ID ? (Sketch*)Rwidget_get_user_handle(g_sketch) : NULL;
	(void)sender;
	(void)user_data;
	if (!s) return;
	if (strncmp(action, "sketch.color:#", 14) == 0 && strlen(action) >= 20) {
		const char* h = action + 14;
		s->r = (hex_digit(h[0]) * 16.0f + hex_digit(h[1])) / 255.0f;
		s->g = (hex_digit(h[2]) * 16.0f + hex_digit(h[3])) / 255.0f;
		s->b = (hex_digit(h[4]) * 16.0f + hex_digit(h[5])) / 255.0f;
	} else if (strcmp(action, "sketch.width") == 0) {
		s->width = Rslider_get_value(sender);
	} else if (strcmp(action, "sketch.undo") == 0 && s->stroke_count > 0) {
		s->stroke_count--;
		s->points = s->strokes[s->stroke_count].first;
	} else if (strcmp(action, "sketch.clear") == 0) {
		s->stroke_count = 0;
		s->points = 0;
	}
	Rwidget_invalidate(g_sketch);
}

static int no_attribute(Rid widget, const char* name, const char* value, void* user_data) {
	(void)widget;
	(void)name;
	(void)value;
	(void)user_data;
	return R_FALSE; /* Name, Height, Slot... are Raven's */
}

void canvas_demo_register(void) {
	static const char* const colors[] = { "sketch.color:#1E1E28", "sketch.color:#E5484D", "sketch.color:#3E63DD", "sketch.color:#30A46C", "sketch.color:#F76B15", "sketch.color:#8E4EC6" };
	size_t i;
	Rxml_register_tag("CanvasShowcase", showcase_create, no_attribute, NULL);
	Rxml_register_tag("SketchPad", sketch_create, no_attribute, NULL);
	for (i = 0; i < sizeof(colors) / sizeof(colors[0]); i++) Raction_bind(colors[i], on_sketch, NULL);
	Raction_bind("sketch.width", on_sketch, NULL);
	Raction_bind("sketch.undo", on_sketch, NULL);
	Raction_bind("sketch.clear", on_sketch, NULL);
}
