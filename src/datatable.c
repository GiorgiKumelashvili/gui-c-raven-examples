/*
 * <DataTable Name="..." Rows="24" Striped="true" OnChanged="action"/>
 *
 * Raven has a <Table> (resizable, sortable columns of text). This one is
 * painted by the application to add what it lacks:
 *   - rows toggled with a check box (and all of them from the header),
 *   - rows that unfold to show their details (the chevron),
 *   - colored tiles in the cells: a status pill, a priority tile, a heat cell,
 *     a progress bar,
 *   - columns resized by dragging the edge of their header (a double click
 *     fits the column to its texts) and sorted by a click on the header,
 *   - a header that stays while the rows scroll, both ways.
 *
 * Raven gives no mouse capture to the application, so a column being
 * dragged follows the window event hook (app_add_event_hook).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#define DT_MAX_ROWS 64

enum {
	COL_ID,
	COL_NAME,
	COL_OWNER,
	COL_STATUS,
	COL_PRIORITY,
	COL_PROGRESS,
	COL_LOAD,
	COL_REGION,
	COL_UPDATED,
	COL_COUNT
};

static const char* const k_headers[COL_COUNT] = { "ID", "Service", "Owner", "Status", "Priority", "Progress", "Load", "Region", "Updated" };
static const float k_default_widths[COL_COUNT] = { 70, 170, 130, 112, 104, 150, 84, 104, 110 };

enum { ST_ACTIVE, ST_PENDING, ST_FAILED, ST_PAUSED, ST_REVIEW, ST_COUNT };
static const char* const k_status[ST_COUNT] = { "Active", "Pending", "Failed", "Paused", "Review" };
static const float k_status_rgb[ST_COUNT][3] = {
	{ 0.20f, 0.74f, 0.42f }, { 0.96f, 0.65f, 0.14f }, { 0.91f, 0.30f, 0.30f }, { 0.55f, 0.58f, 0.63f }, { 0.58f, 0.40f, 0.95f }
};
static const char* const k_priority[4] = { "P0", "P1", "P2", "P3" };
static const float k_priority_rgb[4][3] = {
	{ 0.86f, 0.20f, 0.27f }, { 0.95f, 0.45f, 0.13f }, { 0.20f, 0.52f, 0.96f }, { 0.42f, 0.47f, 0.55f }
};

typedef struct DtRow {
	char id[16];
	const char* name;
	const char* owner;
	int status;
	int priority;
	float progress;
	int load;
	const char* region;
	char updated[16];
	const char* notes;
} DtRow;

typedef struct DataTable {
	Rid widget;
	DtRow rows[DT_MAX_ROWS];
	int order[DT_MAX_ROWS];
	/* the order given by the user (dragged rows), shown when no column sorts */
	int manual[DT_MAX_ROWS];
	unsigned char checked[DT_MAX_ROWS];
	unsigned char expanded[DT_MAX_ROWS];
	RSpring unfold[DT_MAX_ROWS];
	int row_count;
	float col_w[COL_COUNT];
	int sort_col;
	int sort_desc;
	int striped;
	float scroll_x, scroll_y;
	float width, height;
	int hover_row;
	int hover_grip;
	int hover_edge;
	int hover_header;
	int drag_col;
	float drag_start_x, drag_start_w;
	/* a row dragged by its grip: its place in the shown order, where it would go, the pointer */
	int row_from;
	int row_to;
	float row_y;
} DataTable;

static DataTable* g_row_dragging;

static DataTable* g_dragging;

#define HEADER_H 36.0f
#define ROW_H 36.0f
#define DETAIL_H 62.0f
#define GUTTER 84.0f
#define GRIP_END 22.0f
#define CHEVRON_END 42.0f
#define CHECK_X 52.0f
#define EDGE_GRAB 5.0f

/* ---------------------------------------------------------------------------
 *  Data
 * ------------------------------------------------------------------------- */

static void dt_fill(DataTable* dt, int count) {
	static const char* const names[] = {
		"auth-gateway", "billing-api", "search-indexer", "media-transcoder", "notify-worker", "ledger-sync",
		"edge-cache", "report-builder", "fraud-scorer", "session-store", "geo-resolver", "audit-trail",
		"queue-broker", "image-resizer", "feature-flags", "metrics-agent", "payout-engine", "kyc-checker",
		"invoice-render", "schema-registry", "rate-limiter", "mail-relay", "pdf-export", "vault-proxy"
	};
	static const char* const owners[] = { "Ada L.", "Grace H.", "Linus T.", "Margaret H.", "Ken T.", "Barbara L.", "Dennis R.", "Radia P." };
	static const char* const regions[] = { "eu-west-1", "us-east-1", "ap-south-1", "us-west-2", "eu-central-1" };
	static const char* const notes[] = {
		"Rolling deploy in progress; two of five pods updated. Canary error rate is nominal.",
		"Waiting for the schema migration to finish before switching the read replicas.",
		"Health check failing since 09:42 UTC: connection pool exhausted on the primary.",
		"Paused by the on-call engineer during the incident review. Resume after sign-off.",
		"Pull request #482 awaiting review: adds retry jitter and a circuit breaker."
	};
	int i;
	unsigned int seed = 7u;
	if (count < 1) count = 1;
	if (count > DT_MAX_ROWS) count = DT_MAX_ROWS;
	dt->row_count = count;
	for (i = 0; i < count; i++) {
		DtRow* row = &dt->rows[i];
		seed = seed * 1103515245u + 12345u;
		snprintf(row->id, sizeof(row->id), "SVC-%03d", 101 + i);
		row->name = names[i % 24];
		row->owner = owners[(seed >> 8) % 8];
		row->status = (int)((seed >> 12) % ST_COUNT);
		row->priority = (int)((seed >> 16) % 4);
		row->progress = (float)((seed >> 4) % 101) / 100.0f;
		row->load = (int)((seed >> 20) % 100);
		row->region = regions[(seed >> 6) % 5];
		snprintf(row->updated, sizeof(row->updated), "2026-11-%02d", 1 + (int)((seed >> 10) % 7));
		row->notes = notes[row->status];
		dt->order[i] = i;
		dt->manual[i] = i;
		dt->checked[i] = 0;
		dt->expanded[i] = 0;
		memset(&dt->unfold[i], 0, sizeof(RSpring));
	}
}

static const char* dt_cell_text(const DtRow* row, int col, char* buffer, size_t size) {
	switch (col) {
	case COL_ID: return row->id;
	case COL_NAME: return row->name;
	case COL_OWNER: return row->owner;
	case COL_STATUS: return k_status[row->status];
	case COL_PRIORITY: return k_priority[row->priority];
	case COL_PROGRESS: snprintf(buffer, size, "%d%%", (int)(row->progress * 100.0f + 0.5f)); return buffer;
	case COL_LOAD: snprintf(buffer, size, "%d%%", row->load); return buffer;
	case COL_REGION: return row->region;
	case COL_UPDATED: return row->updated;
	default: return "";
	}
}

static DataTable* g_sorting;

static int dt_compare(const void* pa, const void* pb) {
	const DtRow* a = &g_sorting->rows[*(const int*)pa];
	const DtRow* b = &g_sorting->rows[*(const int*)pb];
	int result;
	switch (g_sorting->sort_col) {
	case COL_STATUS: result = a->status - b->status; break;
	case COL_PRIORITY: result = a->priority - b->priority; break;
	case COL_PROGRESS: result = a->progress < b->progress ? -1 : a->progress > b->progress; break;
	case COL_LOAD: result = a->load - b->load; break;
	default: {
		char ba[32], bb[32];
		result = strcmp(dt_cell_text(a, g_sorting->sort_col, ba, sizeof(ba)), dt_cell_text(b, g_sorting->sort_col, bb, sizeof(bb)));
	}
	}
	if (result == 0) result = strcmp(a->id, b->id);
	return g_sorting->sort_desc ? -result : result;
}

static void dt_sort(DataTable* dt) {
	int i;
	for (i = 0; i < dt->row_count; i++) dt->order[i] = dt->manual[i];
	if (dt->sort_col < 0) return;
	g_sorting = dt;
	qsort(dt->order, (size_t)dt->row_count, sizeof(int), dt_compare);
	g_sorting = NULL;
}

/* ---------------------------------------------------------------------------
 *  Geometry
 * ------------------------------------------------------------------------- */

static float dt_columns_width(const DataTable* dt) {
	float total = GUTTER;
	int i;
	for (i = 0; i < COL_COUNT; i++) total += dt->col_w[i];
	return total;
}

static float dt_row_height(const DataTable* dt, int row) {
	return ROW_H + DETAIL_H * dt->unfold[row].value;
}

static float dt_rows_height(const DataTable* dt) {
	float total = 0.0f;
	int i;
	for (i = 0; i < dt->row_count; i++) total += ROW_H + (dt->expanded[i] ? DETAIL_H : 0.0f);
	return total;
}

static void dt_clamp_scroll(DataTable* dt) {
	float max_x = dt_columns_width(dt) - dt->width;
	float max_y = dt_rows_height(dt) - (dt->height - HEADER_H);
	if (max_x < 0.0f) max_x = 0.0f;
	if (max_y < 0.0f) max_y = 0.0f;
	if (dt->scroll_x > max_x) dt->scroll_x = max_x;
	if (dt->scroll_y > max_y) dt->scroll_y = max_y;
	if (dt->scroll_x < 0.0f) dt->scroll_x = 0.0f;
	if (dt->scroll_y < 0.0f) dt->scroll_y = 0.0f;
}

/* The left edge of a column, in the coordinates of the widget. */
static float dt_column_x(const DataTable* dt, int col) {
	float x = GUTTER - dt->scroll_x;
	int i;
	for (i = 0; i < col; i++) x += dt->col_w[i];
	return x;
}

/* The column whose right edge is under x, or -1. */
static int dt_edge_at(const DataTable* dt, float x) {
	int i;
	for (i = 0; i < COL_COUNT; i++) {
		float edge = dt_column_x(dt, i + 1);
		if (edge < GUTTER) continue;
		if (fabsf(x - edge) <= EDGE_GRAB) return i;
	}
	return -1;
}

static int dt_column_at(const DataTable* dt, float x) {
	int i;
	if (x < GUTTER) return -1;
	for (i = 0; i < COL_COUNT; i++) {
		if (x >= dt_column_x(dt, i) && x < dt_column_x(dt, i + 1)) return i;
	}
	return -1;
}

/* The position in the shown order of the row under y (below the header), or -1; *in_detail tells the unfolded part. */
static int dt_row_at(const DataTable* dt, float y, int* in_detail) {
	float top = HEADER_H - dt->scroll_y;
	int i;
	if (y < HEADER_H) return -1;
	for (i = 0; i < dt->row_count; i++) {
		int row = dt->order[i];
		float h = dt_row_height(dt, row);
		if (y >= top && y < top + h) {
			if (in_detail) *in_detail = y >= top + ROW_H;
			return i;
		}
		top += h;
	}
	return -1;
}

static void dt_autofit(DataTable* dt, int col) {
	char buffer[32];
	float widest, w, h;
	int i;
	Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), k_headers[col], &widest, &h);
	widest += 22.0f;
	for (i = 0; i < dt->row_count; i++) {
		Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), dt_cell_text(&dt->rows[i], col, buffer, sizeof(buffer)), &w, &h);
		if (col == COL_STATUS) w += 26.0f;
		if (col == COL_PROGRESS) w += 80.0f;
		if (w + 24.0f > widest) widest = w + 24.0f;
	}
	dt->col_w[col] = widest;
}

/* ---------------------------------------------------------------------------
 *  Painting
 * ------------------------------------------------------------------------- */

typedef struct Rgba {
	float r, g, b, a;
} Rgba;

static Rgba dt_color(Rid widget, RColorRole role) {
	Rgba c;
	Rwidget_get_color(widget, role, &c.r, &c.g, &c.b, &c.a);
	return c;
}

/* A theme color made opaque over the window (the macOS looks give translucent ones). */
static Rgba dt_solid(Rid widget, RColorRole role) {
	Rgba c = dt_color(widget, role), w = dt_color(widget, R_COLOR_WINDOW);
	Rgba out;
	out.r = c.r * c.a + w.r * (1.0f - c.a);
	out.g = c.g * c.a + w.g * (1.0f - c.a);
	out.b = c.b * c.a + w.b * (1.0f - c.a);
	out.a = 1.0f;
	return out;
}

static void dt_checkbox(RPainter* p, Rid widget, float x, float y, int state) {
	/* state: 0 off, 1 on, 2 some */
	Rgba border = dt_color(widget, R_COLOR_CONTROL_BORDER);
	Rgba accent = dt_color(widget, R_COLOR_ACCENT);
	Rgba mark = dt_color(widget, R_COLOR_ACCENT_TEXT);
	const float s = 16.0f;
	if (state) {
		Rdraw_rect(p, x, y, s, s, 4.0f, accent.r, accent.g, accent.b, 1.0f);
		if (state == 1) {
			Rdraw_line(p, x + 4.0f, y + 8.5f, x + 7.0f, y + 11.5f, 1.8f, mark.r, mark.g, mark.b, 1.0f);
			Rdraw_line(p, x + 7.0f, y + 11.5f, x + 12.0f, y + 4.8f, 1.8f, mark.r, mark.g, mark.b, 1.0f);
		} else {
			Rdraw_line(p, x + 4.0f, y + 8.0f, x + 12.0f, y + 8.0f, 2.0f, mark.r, mark.g, mark.b, 1.0f);
		}
	} else {
		Rgba input = dt_color(widget, R_COLOR_INPUT);
		Rdraw_rect(p, x, y, s, s, 4.0f, input.r, input.g, input.b, input.a);
		Rdraw_rect_outline(p, x, y, s, s, 4.0f, 1.2f, border.r, border.g, border.b, border.a);
	}
}

static void dt_chevron(RPainter* p, float cx, float cy, float open, Rgba c) {
	/* a chevron that turns from right (0) to down (1) */
	float angle = open * 1.5707963f;
	float ca = cosf(angle), sa = sinf(angle);
	float pts[3][2] = { { -2.5f, -5.0f }, { 2.5f, 0.0f }, { -2.5f, 5.0f } };
	float out[3][2];
	int i;
	for (i = 0; i < 3; i++) {
		out[i][0] = cx + pts[i][0] * ca - pts[i][1] * sa;
		out[i][1] = cy + pts[i][0] * sa + pts[i][1] * ca;
	}
	Rdraw_line(p, out[0][0], out[0][1], out[1][0], out[1][1], 1.6f, c.r, c.g, c.b, c.a);
	Rdraw_line(p, out[1][0], out[1][1], out[2][0], out[2][1], 1.6f, c.r, c.g, c.b, c.a);
}

static float g_text_h;

static void dt_text(RPainter* p, float x, float y, float row_h, const char* text, Rgba c, float alpha) {
	Rdraw_text(p, R_INVALID_ID, 0.0f, x, y + (row_h - g_text_h) * 0.5f, text, c.r, c.g, c.b, c.a * alpha);
}

static void dt_paint_cell(DataTable* dt, RPainter* p, const DtRow* row, int col, float x, float y, float w, Rgba text) {
	char buffer[32];
	float tw = 0.0f, th = 0.0f;
	const char* label = dt_cell_text(row, col, buffer, sizeof(buffer));
	const float pad = 12.0f;
	Rdraw_push_clip_rect(p, x, y, w, ROW_H);
	switch (col) {
	case COL_STATUS: {
		/* a pill: a dot and the word, on a wash of the color */
		const float* rgb = k_status_rgb[row->status];
		Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), label, &tw, &th);
		Rdraw_rect(p, x + pad - 4.0f, y + (ROW_H - 22.0f) * 0.5f, tw + 26.0f, 22.0f, 11.0f, rgb[0], rgb[1], rgb[2], 0.18f);
		Rdraw_circle(p, x + pad + 5.0f, y + ROW_H * 0.5f, 3.5f, rgb[0], rgb[1], rgb[2], 1.0f);
		Rdraw_text(p, R_INVALID_ID, 0.0f, x + pad + 13.0f, y + (ROW_H - g_text_h) * 0.5f, label, rgb[0], rgb[1], rgb[2], 1.0f);
		break;
	}
	case COL_PRIORITY: {
		/* a solid tile */
		const float* rgb = k_priority_rgb[row->priority];
		Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), label, &tw, &th);
		Rdraw_rect(p, x + pad - 2.0f, y + (ROW_H - 22.0f) * 0.5f, tw + 16.0f, 22.0f, 5.0f, rgb[0], rgb[1], rgb[2], 1.0f);
		Rdraw_text(p, R_INVALID_ID, 0.0f, x + pad + 6.0f, y + (ROW_H - g_text_h) * 0.5f, label, 1.0f, 1.0f, 1.0f, 1.0f);
		break;
	}
	case COL_LOAD: {
		/* a heat cell: green, amber, red */
		float t = (float)row->load / 100.0f;
		float r = t < 0.5f ? 0.20f + t * 1.5f : 0.95f;
		float g = t < 0.5f ? 0.70f : 0.70f - (t - 0.5f) * 1.0f;
		float b = 0.30f - t * 0.15f;
		Rdraw_rect(p, x + 3.0f, y + 3.0f, w - 6.0f, ROW_H - 6.0f, 5.0f, r, g, b, 0.85f);
		Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), label, &tw, &th);
		Rdraw_text(p, R_INVALID_ID, 0.0f, x + (w - tw) * 0.5f, y + (ROW_H - g_text_h) * 0.5f, label, 1.0f, 1.0f, 1.0f, 1.0f);
		break;
	}
	case COL_PROGRESS: {
		Rgba track = dt_color(dt->widget, R_COLOR_TRACK);
		Rgba accent = dt_color(dt->widget, R_COLOR_ACCENT);
		float bar_w = w - pad * 2.0f - 40.0f;
		if (bar_w < 20.0f) bar_w = 20.0f;
		Rdraw_rect(p, x + pad, y + ROW_H * 0.5f - 3.0f, bar_w, 6.0f, 3.0f, track.r, track.g, track.b, track.a);
		Rdraw_rect(p, x + pad, y + ROW_H * 0.5f - 3.0f, bar_w * row->progress, 6.0f, 3.0f, accent.r, accent.g, accent.b, 1.0f);
		dt_text(p, x + pad + bar_w + 8.0f, y, ROW_H, label, text, 0.75f);
		break;
	}
	case COL_ID:
		dt_text(p, x + pad, y, ROW_H, label, text, 0.6f);
		break;
	default:
		dt_text(p, x + pad, y, ROW_H, label, text, 1.0f);
		break;
	}
	Rdraw_pop_clip_rect(p);
}

static int dt_checked_count(const DataTable* dt) {
	int i, n = 0;
	for (i = 0; i < dt->row_count; i++) n += dt->checked[i] ? 1 : 0;
	return n;
}

static void dt_paint(Rid widget, RPainter* p, float width, float height, void* user_data) {
	DataTable* dt = (DataTable*)user_data;
	Rgba bg = dt_solid(widget, R_COLOR_INPUT);
	Rgba header = dt_solid(widget, R_COLOR_HEADER);
	Rgba text = dt_color(widget, R_COLOR_TEXT);
	Rgba line = dt_color(widget, R_COLOR_SEPARATOR);
	Rgba border = dt_color(widget, R_COLOR_BORDER);
	Rgba hover = dt_color(widget, R_COLOR_ROW_HOVERED);
	Rgba selection = dt_color(widget, R_COLOR_SELECTION);
	Rgba accent = dt_color(widget, R_COLOR_ACCENT);
	float radius = Rtheme_get_metric(R_METRIC_CORNER_RADIUS) + 2.0f;
	float content_w, top, dummy;
	int i, col, checked;

	dt->width = width;
	dt->height = height;
	dt_clamp_scroll(dt);
	Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), "Ag", &dummy, &g_text_h);
	content_w = dt_columns_width(dt) - dt->scroll_x;
	if (content_w < width) content_w = width;

	Rdraw_rect(p, 0.0f, 0.0f, width, height, radius, bg.r, bg.g, bg.b, 1.0f);
	Rdraw_push_clip_rect(p, 0.0f, 0.0f, width, height);

	/* ---- rows ---- */
	Rdraw_push_clip_rect(p, 0.0f, HEADER_H, width, height - HEADER_H);
	top = HEADER_H - dt->scroll_y;
	for (i = 0; i < dt->row_count; i++) {
		int row = dt->order[i];
		float unfold = Rdraw_spring(p, &dt->unfold[row], dt->expanded[row] ? 1.0f : 0.0f, R_MOTION_SMOOTH);
		float h = ROW_H + DETAIL_H * unfold;
		if (top + h >= HEADER_H && top <= height) {
			if (dt->checked[row]) Rdraw_rect(p, 0.0f, top, content_w, h, 0.0f, selection.r, selection.g, selection.b, selection.a * 0.45f);
			else if (i == dt->hover_row) Rdraw_rect(p, 0.0f, top, content_w, h, 0.0f, hover.r, hover.g, hover.b, hover.a);
			else if (dt->striped && (i & 1)) Rdraw_rect(p, 0.0f, top, content_w, h, 0.0f, text.r, text.g, text.b, 0.035f);

			/* the cells scroll sideways under the gutter */
			Rdraw_push_clip_rect(p, GUTTER, top, width - GUTTER, ROW_H);
			for (col = 0; col < COL_COUNT; col++) {
				float x = dt_column_x(dt, col);
				if (x + dt->col_w[col] < GUTTER || x > width) continue;
				dt_paint_cell(dt, p, &dt->rows[row], col, x, top, dt->col_w[col], text);
			}
			Rdraw_pop_clip_rect(p);

			/* the gutter: grip, chevron and check box */
			{
				float gy = top + ROW_H * 0.5f, ga = (i == dt->hover_row || i == dt->row_from) ? 0.7f : 0.3f;
				int d;
				for (d = 0; d < 3; d++) {
					Rdraw_circle(p, 9.0f, gy - 5.0f + 5.0f * (float)d, 1.4f, text.r, text.g, text.b, ga);
					Rdraw_circle(p, 14.0f, gy - 5.0f + 5.0f * (float)d, 1.4f, text.r, text.g, text.b, ga);
				}
			}
			dt_chevron(p, 32.0f, top + ROW_H * 0.5f, unfold, text);
			dt_checkbox(p, widget, CHECK_X, top + (ROW_H - 16.0f) * 0.5f, dt->checked[row]);
			if (i == dt->row_from) Rdraw_rect(p, 0.0f, top, content_w, h, 0.0f, accent.r, accent.g, accent.b, 0.10f);

			/* the details, unfolding */
			if (unfold > 0.01f) {
				char line1[160];
				const DtRow* r = &dt->rows[row];
				Rdraw_push_clip_rect(p, 0.0f, top + ROW_H, width, DETAIL_H * unfold);
				Rdraw_rect(p, GUTTER - 8.0f, top + ROW_H + 2.0f, width - GUTTER - 8.0f, DETAIL_H - 10.0f, 6.0f, text.r, text.g, text.b, 0.06f);
				Rdraw_rect(p, GUTTER - 8.0f, top + ROW_H + 2.0f, 3.0f, DETAIL_H - 10.0f, 1.5f,
					k_status_rgb[r->status][0], k_status_rgb[r->status][1], k_status_rgb[r->status][2], 1.0f);
				snprintf(line1, sizeof(line1), "%s  -  %s  -  owned by %s  -  %s", r->id, r->name, r->owner, r->region);
				Rdraw_text(p, R_INVALID_ID, 0.0f, GUTTER + 6.0f, top + ROW_H + 9.0f, line1, text.r, text.g, text.b, text.a);
				Rdraw_text(p, R_INVALID_ID, 0.0f, GUTTER + 6.0f, top + ROW_H + 13.0f + g_text_h, r->notes, text.r, text.g, text.b, text.a * 0.65f);
				Rdraw_pop_clip_rect(p);
			}
			Rdraw_line(p, 0.0f, top + h - 0.5f, content_w, top + h - 0.5f, 1.0f, line.r, line.g, line.b, line.a * 0.6f);
		}
		top += h;
	}
	/* the row being dragged: where it would land, and a ghost under the pointer */
	if (dt->row_from >= 0) {
		float y = HEADER_H - dt->scroll_y;
		const DtRow* r = &dt->rows[dt->order[dt->row_from]];
		char label[80];
		for (i = 0; i < dt->row_to && i < dt->row_count; i++) y += dt_row_height(dt, dt->order[i]);
		Rdraw_rect(p, 4.0f, y - 1.5f, content_w - 8.0f, 3.0f, 1.5f, accent.r, accent.g, accent.b, 1.0f);
		Rdraw_circle(p, 6.0f, y, 4.0f, accent.r, accent.g, accent.b, 1.0f);
		snprintf(label, sizeof(label), "%s  %s", r->id, r->name);
		Rdraw_rect(p, 24.0f, dt->row_y - 14.0f, 240.0f, 28.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.25f);
		Rdraw_rect(p, 22.0f, dt->row_y - 16.0f, 240.0f, 28.0f, 8.0f, accent.r, accent.g, accent.b, 0.95f);
		Rdraw_text(p, R_INVALID_ID, 0.0f, 34.0f, dt->row_y - 2.0f - g_text_h * 0.5f, label, 1.0f, 1.0f, 1.0f, 1.0f);
	}
	Rdraw_pop_clip_rect(p);

	/* ---- header ---- */
	Rdraw_rect(p, 0.0f, 0.0f, width, HEADER_H, 0.0f, header.r, header.g, header.b, header.a);
	Rdraw_push_clip_rect(p, GUTTER, 0.0f, width - GUTTER, HEADER_H);
	for (col = 0; col < COL_COUNT; col++) {
		float x = dt_column_x(dt, col);
		float w = dt->col_w[col];
		float edge = x + w;
		int active_edge = dt->drag_col == col || (dt->drag_col < 0 && dt->hover_edge == col);
		if (col == dt->hover_header && dt->drag_col < 0 && dt->hover_edge < 0) {
			Rdraw_rect(p, x, 0.0f, w, HEADER_H, 0.0f, hover.r, hover.g, hover.b, hover.a);
		}
		Rdraw_push_clip_rect(p, x, 0.0f, w - 4.0f, HEADER_H);
		Rdraw_text(p, R_INVALID_ID, 0.0f, x + 12.0f, (HEADER_H - g_text_h) * 0.5f, k_headers[col], text.r, text.g, text.b, text.a * 0.8f);
		if (col == dt->sort_col) {
			float tw, th, ax, ay = HEADER_H * 0.5f;
			Rfont_measure_text(Rtheme_get_font(), Rtheme_get_metric(R_METRIC_FONT_SIZE), k_headers[col], &tw, &th);
			ax = x + 12.0f + tw + 8.0f;
			if (dt->sort_desc) Rdraw_triangle(p, ax, ay - 2.5f, ax + 8.0f, ay - 2.5f, ax + 4.0f, ay + 2.5f, accent.r, accent.g, accent.b, 1.0f);
			else Rdraw_triangle(p, ax, ay + 2.5f, ax + 8.0f, ay + 2.5f, ax + 4.0f, ay - 2.5f, accent.r, accent.g, accent.b, 1.0f);
		}
		Rdraw_pop_clip_rect(p);
		if (active_edge) Rdraw_line(p, edge - 1.0f, 4.0f, edge - 1.0f, height - 4.0f, 2.0f, accent.r, accent.g, accent.b, 0.9f);
		else Rdraw_line(p, edge - 0.5f, 9.0f, edge - 0.5f, HEADER_H - 9.0f, 1.0f, line.r, line.g, line.b, line.a);
	}
	Rdraw_pop_clip_rect(p);
	checked = dt_checked_count(dt);
	dt_checkbox(p, widget, CHECK_X, (HEADER_H - 16.0f) * 0.5f, checked == 0 ? 0 : checked == dt->row_count ? 1 : 2);
	Rdraw_line(p, 0.0f, HEADER_H - 0.5f, width, HEADER_H - 0.5f, 1.0f, border.r, border.g, border.b, border.a);
	if (dt->scroll_x > 0.0f) Rdraw_rect(p, GUTTER, 0.0f, 6.0f, height, 0.0f, 0.0f, 0.0f, 0.0f, 0.12f);

	/* ---- scroll bars ---- */
	{
		float view_h = height - HEADER_H, rows_h = dt_rows_height(dt), all_w = dt_columns_width(dt);
		if (rows_h > view_h) {
			float bar = view_h * view_h / rows_h;
			float y = HEADER_H + (view_h - bar) * (dt->scroll_y / (rows_h - view_h));
			Rdraw_rect(p, width - 6.0f, y + 2.0f, 4.0f, bar - 4.0f, 2.0f, text.r, text.g, text.b, 0.25f);
		}
		if (all_w > width) {
			float bar = width * width / all_w;
			float x = (width - bar) * (dt->scroll_x / (all_w - width));
			Rdraw_rect(p, x + 2.0f, height - 6.0f, bar - 4.0f, 4.0f, 2.0f, text.r, text.g, text.b, 0.25f);
		}
	}
	Rdraw_pop_clip_rect(p);
	Rdraw_rect_outline(p, 0.0f, 0.0f, width, height, radius, 1.0f, border.r, border.g, border.b, border.a);
}

/* ---------------------------------------------------------------------------
 *  Input
 * ------------------------------------------------------------------------- */

static void dt_changed(DataTable* dt) {
	const char* action = Rwidget_get_action(dt->widget);
	Rwidget_invalidate(dt->widget);
	if (action && action[0]) Raction_fire(action, dt->widget);
}

static int dt_event(Rid widget, const REvent* event, void* user_data) {
	DataTable* dt = (DataTable*)user_data;
	float x = event->local_x, y = event->local_y;
	(void)widget;
	switch (event->type) {
	case R_EVENT_MOUSE_MOVE: {
		int edge = y < HEADER_H ? dt_edge_at(dt, x) : -1;
		int header = y < HEADER_H ? dt_column_at(dt, x) : -1;
		int row = dt_row_at(dt, y, NULL);
		int grip = row >= 0 && x < GRIP_END;
		if (edge != dt->hover_edge || header != dt->hover_header || row != dt->hover_row || grip != dt->hover_grip) {
			dt->hover_grip = grip;
			dt->hover_edge = edge;
			dt->hover_header = header;
			dt->hover_row = row;
			Rwidget_set_cursor(dt->widget, edge >= 0 ? R_CURSOR_RESIZE_H : (row >= 0 && x < GRIP_END) ? R_CURSOR_MOVE : (y < HEADER_H || row >= 0) ? R_CURSOR_HAND : R_CURSOR_ARROW);
			Rwidget_invalidate(dt->widget);
		}
		return R_FALSE;
	}
	case R_EVENT_MOUSE_LEAVE:
		if (dt->drag_col < 0) {
			dt->hover_edge = dt->hover_header = dt->hover_row = -1;
			Rwidget_invalidate(dt->widget);
		}
		return R_FALSE;
	case R_EVENT_MOUSE_DOWN: {
		int edge, col, index, in_detail = 0;
		if (event->button != R_MOUSE_LEFT) return R_FALSE;
		if (y < HEADER_H) {
			edge = dt_edge_at(dt, x);
			if (edge >= 0) {
				if (event->clicks >= 2) {
					dt_autofit(dt, edge);
					Rwidget_invalidate(dt->widget);
				} else {
					dt->drag_col = edge;
					dt->drag_start_x = event->x;
					dt->drag_start_w = dt->col_w[edge];
					g_dragging = dt;
				}
				return R_TRUE;
			}
			if (x >= CHEVRON_END && x < GUTTER) {
				/* the check box of the header: all, or none */
				int all = dt_checked_count(dt) != dt->row_count, i;
				for (i = 0; i < dt->row_count; i++) dt->checked[i] = (unsigned char)all;
				dt_changed(dt);
				return R_TRUE;
			}
			col = dt_column_at(dt, x);
			if (col >= 0) {
				if (dt->sort_col == col && !dt->sort_desc) dt->sort_desc = 1;
				else if (dt->sort_col == col) dt->sort_col = -1;
				else {
					dt->sort_col = col;
					dt->sort_desc = 0;
				}
				dt_sort(dt);
				Rwidget_invalidate(dt->widget);
			}
			return R_TRUE;
		}
		index = dt_row_at(dt, y, &in_detail);
		if (index < 0 || in_detail) return R_TRUE;
		if (x < GRIP_END) {
			/* the grip: the row follows the pointer (dt_hook) */
			dt->row_from = dt->row_to = index;
			dt->row_y = y;
			g_row_dragging = dt;
			Rwidget_invalidate(dt->widget);
			return R_TRUE;
		}
		if (x < CHEVRON_END || event->clicks >= 2) {
			int row = dt->order[index];
			dt->expanded[row] = (unsigned char)!dt->expanded[row];
			Rwidget_invalidate(dt->widget);
		} else {
			int row = dt->order[index];
			dt->checked[row] = (unsigned char)!dt->checked[row];
			dt_changed(dt);
		}
		return R_TRUE;
	}
	case R_EVENT_MOUSE_WHEEL: {
		float step = event->wheel_precise ? 48.0f : 40.0f;
		float old_x = dt->scroll_x, old_y = dt->scroll_y;
		float wx = event->wheel_x, wy = event->wheel_y;
		/* shift + wheel scrolls sideways */
		if ((event->modifiers & R_MOD_SHIFT) && wx == 0.0f) {
			wx = wy;
			wy = 0.0f;
		}
		dt->scroll_x -= wx * step;
		dt->scroll_y -= wy * step;
		dt_clamp_scroll(dt);
		if (dt->scroll_x == old_x && dt->scroll_y == old_y) return R_FALSE; /* at the end: the page scrolls */
		dt->hover_row = dt_row_at(dt, y, NULL);
		Rwidget_invalidate(dt->widget);
		return R_TRUE;
	}
	default:
		return R_FALSE;
	}
}

/* Sees the window events while a column is dragged: the pointer may leave the table. */
/* The place, in the shown order, a dragged row goes to: before the row whose middle is below y. */
static int dt_insertion_at(const DataTable* dt, float y) {
	float top = HEADER_H - dt->scroll_y;
	int i;
	for (i = 0; i < dt->row_count; i++) {
		float h = dt_row_height(dt, dt->order[i]);
		if (y < top + h * 0.5f) return i;
		top += h;
	}
	return dt->row_count;
}

static int dt_row_hook(const REvent* event) {
	DataTable* dt = g_row_dragging;
	float wx, wy, ww, wh;
	Rwidget_get_rect(dt->widget, &wx, &wy, &ww, &wh);
	if (event->type == R_EVENT_MOUSE_MOVE) {
		dt->row_y = event->y - wy;
		/* near an edge, the rows scroll under the pointer */
		if (dt->row_y < HEADER_H + 24.0f) dt->scroll_y -= 10.0f;
		else if (dt->row_y > dt->height - 24.0f) dt->scroll_y += 10.0f;
		dt_clamp_scroll(dt);
		dt->row_to = dt_insertion_at(dt, dt->row_y);
		Rwidget_invalidate(dt->widget);
		return R_TRUE;
	}
	if (event->type == R_EVENT_MOUSE_UP && event->button == R_MOUSE_LEFT) {
		int from = dt->row_from, to = dt->row_to, moved, i;
		if (to > from) to--;
		if (to != from && from >= 0) {
			moved = dt->order[from];
			if (to < from) memmove(&dt->order[to + 1], &dt->order[to], sizeof(int) * (size_t)(from - to));
			else memmove(&dt->order[from], &dt->order[from + 1], sizeof(int) * (size_t)(to - from));
			dt->order[to] = moved;
			/* the order shown becomes the order of the user: no column sorts any more */
			for (i = 0; i < dt->row_count; i++) dt->manual[i] = dt->order[i];
			dt->sort_col = -1;
		}
		dt->row_from = dt->row_to = -1;
		g_row_dragging = NULL;
		dt_changed(dt);
		return R_TRUE;
	}
	return R_FALSE;
}

static int dt_hook(const REvent* event) {
	DataTable* dt = g_dragging;
	float width;
	if (g_row_dragging) return dt_row_hook(event);
	if (!dt) return R_FALSE;
	if (event->type == R_EVENT_MOUSE_MOVE) {
		width = dt->drag_start_w + (event->x - dt->drag_start_x);
		if (width < 48.0f) width = 48.0f;
		if (width > 600.0f) width = 600.0f;
		dt->col_w[dt->drag_col] = width;
		Rwidget_invalidate(dt->widget);
		return R_TRUE;
	}
	if (event->type == R_EVENT_MOUSE_UP && event->button == R_MOUSE_LEFT) {
		dt->drag_col = -1;
		g_dragging = NULL;
		Rwidget_invalidate(dt->widget);
		return R_TRUE;
	}
	return R_FALSE;
}

/* ---------------------------------------------------------------------------
 *  Tag
 * ------------------------------------------------------------------------- */

static void dt_measure(Rid widget, float* width, float* height, void* user_data) {
	(void)widget;
	(void)user_data;
	*width = 420.0f;
	*height = 360.0f;
}

static void dt_destroy(Rid widget, void* user_data) {
	DataTable* dt = (DataTable*)user_data;
	(void)widget;
	if (g_dragging == dt) g_dragging = NULL;
	if (g_row_dragging == dt) g_row_dragging = NULL;
	free(dt);
}

static Rid dt_create(const char* tag, void* user_data) {
	static int hooked;
	Rid widget = Rwidget_create(R_WIDGET_CUSTOM);
	DataTable* dt = (DataTable*)calloc(1, sizeof(DataTable));
	int i;
	(void)tag;
	(void)user_data;
	if (!hooked) {
		app_add_event_hook(dt_hook);
		hooked = 1;
	}
	dt->widget = widget;
	dt->sort_col = -1;
	dt->striped = 1;
	dt->drag_col = dt->hover_edge = dt->hover_header = dt->hover_row = -1;
	dt->row_from = dt->row_to = -1;
	for (i = 0; i < COL_COUNT; i++) dt->col_w[i] = k_default_widths[i];
	dt_fill(dt, 24);
	Rwidget_set_user_handle(widget, dt);
	Rwidget_set_clipping(widget, R_TRUE);
	Rcustom_set_measure_callback(widget, dt_measure, dt);
	Rcustom_set_paint_callback(widget, dt_paint, dt);
	Rwidget_set_event_callback(widget, dt_event, dt);
	Rwidget_set_destroy_callback(widget, dt_destroy, dt);
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

static int dt_attribute(Rid widget, const char* name, const char* value, void* user_data) {
	DataTable* dt = (DataTable*)Rwidget_get_user_handle(widget);
	(void)user_data;
	if (!dt) return R_FALSE;
	if (same(name, "Rows")) dt_fill(dt, atoi(value));
	else if (same(name, "Striped")) dt->striped = same(value, "true") || same(value, "1");
	else return R_FALSE;
	Rwidget_invalidate(widget);
	return R_TRUE;
}

void datatable_register(void) {
	Rxml_register_tag("DataTable", dt_create, dt_attribute, NULL);
}

int datatable_checked_count(Rid widget) {
	DataTable* dt = (DataTable*)Rwidget_get_user_handle(widget);
	return dt ? dt_checked_count(dt) : 0;
}

int datatable_row_count(Rid widget) {
	DataTable* dt = (DataTable*)Rwidget_get_user_handle(widget);
	return dt ? dt->row_count : 0;
}

void datatable_command(Rid widget, const char* command) {
	DataTable* dt = (DataTable*)Rwidget_get_user_handle(widget);
	int i;
	if (!dt) return;
	for (i = 0; i < dt->row_count; i++) {
		if (strcmp(command, "check-all") == 0) dt->checked[i] = 1;
		else if (strcmp(command, "check-none") == 0) dt->checked[i] = 0;
		else if (strcmp(command, "invert") == 0) dt->checked[i] = (unsigned char)!dt->checked[i];
		else if (strcmp(command, "expand-all") == 0) dt->expanded[i] = 1;
		else if (strcmp(command, "collapse-all") == 0) dt->expanded[i] = 0;
	}
	if (strcmp(command, "reset-order") == 0) {
		for (i = 0; i < dt->row_count; i++) dt->manual[i] = i;
		dt->sort_col = -1;
		dt_sort(dt);
	}
	if (strcmp(command, "reset-columns") == 0) {
		for (i = 0; i < COL_COUNT; i++) dt->col_w[i] = k_default_widths[i];
		dt->sort_col = -1;
		dt_sort(dt);
	}
	dt_changed(dt);
}
