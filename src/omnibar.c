/*
 * Omni bar (custom): one field that finds pages and runs commands.
 *
 * Two of them share this code: the command palette, laid over the window by
 * main.xml and opened with Cmd+K, and the one embedded in the Omni bar page.
 * Each is a TextInput over a ListView: typing filters (every word must
 * appear, titles that start with the text come first), Up / Down move in
 * the results, Enter or a click runs the one selected, Escape closes.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "app.h"

typedef struct OmniItem {
	const char* label;   /* page title or command */
	const char* detail;  /* section, or "Command" */
	const char* page;    /* page id, or NULL */
	const char* action;  /* action name, or NULL */
} OmniItem;

static const OmniItem k_commands[] = {
	{ "Theme: macOS Dark", "Command", NULL, "theme:MacOSDark" },
	{ "Theme: macOS Light", "Command", NULL, "theme:MacOSLight" },
	{ "Theme: macOS Liquid Glass Dark", "Command", NULL, "theme:MacOSGlassDark" },
	{ "Theme: Raven Dark", "Command", NULL, "theme:RavenDark" },
	{ "Theme: Raven Light", "Command", NULL, "theme:RavenLight" },
	{ "Theme: Windows 11 Light", "Command", NULL, "theme:WindowsLight" },
	{ "Toggle dark / light", "Command", NULL, "theme.toggle" },
	{ "Toggle sidebar", "Command", NULL, "toggle:sidebar" },
	{ "Go back", "Command", NULL, "nav.back" },
	{ "Go forward", "Command", NULL, "nav.forward" },
	{ "Send a macOS notification", "Command", NULL, "notify.simple" },
	{ "Show a toast", "Command", NULL, "say:Hello from the omni bar" },
	{ "Open Raven on GitHub", "Command", NULL, "open:https://github.com/oscar-soirey/Raven" },
};
#define COMMAND_COUNT ((int)(sizeof(k_commands) / sizeof(k_commands[0])))
#define MAX_ITEMS 128

static OmniItem g_items[MAX_ITEMS];
static int g_item_count;

typedef struct Omni {
	const char* input;
	const char* list;
	int palette;
	int results[MAX_ITEMS];
	int count;
} Omni;

static Omni g_palette = { "omni_input", "omni_list", 1, { 0 }, 0 };
static Omni g_inline = { "omni_page_input", "omni_page_list", 0, { 0 }, 0 };

static void build_items(void) {
	int i, n = nav_entry_count();
	g_item_count = 0;
	for (i = 0; i < n && g_item_count < MAX_ITEMS; i++) {
		OmniItem* item = &g_items[g_item_count++];
		item->label = nav_entry_title(i);
		item->detail = nav_entry_section(i);
		item->page = nav_entry_id(i);
		item->action = NULL;
	}
	for (i = 0; i < COMMAND_COUNT && g_item_count < MAX_ITEMS; i++) g_items[g_item_count++] = k_commands[i];
}

static int find_nocase(const char* text, const char* part, size_t length) {
	size_t i, j;
	if (length == 0) return 0;
	for (i = 0; text[i]; i++) {
		for (j = 0; j < length && text[i + j]; j++) {
			if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)part[j])) break;
		}
		if (j == length) return (int)i;
	}
	return -1;
}

/* Every word of the query appears in the label or the detail. */
static int matches(const OmniItem* item, const char* query) {
	const char* word = query;
	while (*word) {
		size_t length;
		while (*word == ' ') word++;
		length = strcspn(word, " ");
		if (length == 0) break;
		if (find_nocase(item->label, word, length) < 0 && find_nocase(item->detail, word, length) < 0) return 0;
		word += length;
	}
	return 1;
}

static void filter(Omni* omni, const char* query) {
	Rid list = Rwidget_find(g_window, omni->list);
	int pass, i;
	if (list == R_INVALID_ID) return;
	if (!query) query = "";
	omni->count = 0;
	/* the titles that start with the query, then the others */
	for (pass = 0; pass < 2; pass++) {
		for (i = 0; i < g_item_count; i++) {
			int starts = query[0] && find_nocase(g_items[i].label, query, strlen(query)) == 0;
			if (!matches(&g_items[i], query) || (pass == 0) != starts) continue;
			if (!query[0] && pass == 0) continue;
			omni->results[omni->count++] = i;
		}
	}
	{
		const char* titles[MAX_ITEMS];
		const char* details[MAX_ITEMS];
		for (i = 0; i < omni->count; i++) {
			titles[i] = g_items[omni->results[i]].label;
			details[i] = g_items[omni->results[i]].detail;
		}
		roundlist_set_items(list, omni->count, titles, details);
	}
	roundlist_set_selected(list, omni->count > 0 ? 0 : -1);
}

static void close_palette(void) {
	Rwidget_set_visibility(Rwidget_find(g_window, "omni"), R_COLLAPSED);
	Rwidget_set_visibility(Rwidget_find(g_window, "omni_scrim"), R_COLLAPSED);
}

static int palette_open(void) {
	Rid omni = Rwidget_find(g_window, "omni");
	return omni != R_INVALID_ID && Rwidget_get_visibility(omni) == R_VISIBLE;
}

void omnibar_toggle(void) {
	Rid input = Rwidget_find(g_window, g_palette.input);
	if (palette_open()) {
		close_palette();
		return;
	}
	popover_close();
	Rwidget_set_visibility(Rwidget_find(g_window, "omni_scrim"), R_VISIBLE);
	Rwidget_set_visibility(Rwidget_find(g_window, "omni"), R_VISIBLE);
	Rtext_input_set_text(input, "");
	filter(&g_palette, "");
	Rwidget_focus(input);
}

static void run(Omni* omni) {
	Rid list = Rwidget_find(g_window, omni->list);
	Rid input = Rwidget_find(g_window, omni->input);
	int index = list != R_INVALID_ID ? roundlist_get_selected(list) : -1;
	const OmniItem* item;
	if (index < 0 || index >= omni->count) return;
	item = &g_items[omni->results[index]];
	if (omni->palette) close_palette();
	if (item->page) nav_go(item->page);
	else Raction_fire(item->action, input);
}

static void on_changed(Rid input, const char* text, void* user_data) {
	(void)input;
	filter((Omni*)user_data, text);
}

static void on_committed(Rid input, const char* text, void* user_data) {
	(void)input;
	(void)text;
	run((Omni*)user_data);
}

static void on_activated(Rid list, int index, void* user_data) {
	(void)list;
	(void)index;
	run((Omni*)user_data);
}

static void move_selection(Omni* omni, int step) {
	Rid list = Rwidget_find(g_window, omni->list);
	int index;
	if (list == R_INVALID_ID || omni->count == 0) return;
	index = roundlist_get_selected(list) + step;
	if (index < 0) index = omni->count - 1;
	if (index >= omni->count) index = 0;
	roundlist_set_selected(list, index);
}

static int inside(const char* name, float x, float y) {
	float rx, ry, rw, rh;
	Rid widget = Rwidget_find(g_window, name);
	if (widget == R_INVALID_ID) return 0;
	Rwidget_get_rect(widget, &rx, &ry, &rw, &rh);
	return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static int omni_key(Omni* omni, RKey key);

static int omni_hook(const REvent* event) {
	Omni* omni = NULL;
	if (event->type == R_EVENT_MOUSE_DOWN && palette_open() && !inside("omni", event->x, event->y)) {
		close_palette();
		return R_TRUE; /* as a modal: the click only closes */
	}
	if (event->type != R_EVENT_KEY_DOWN) return R_FALSE;
	/* the palette is modal: its keys, wherever the focus is */
	if (palette_open()) omni = &g_palette;
	else if (Rwidget_is_focused(Rwidget_find(g_window, g_inline.input)) || Rwidget_is_focused(Rwidget_find(g_window, g_inline.list))) omni = &g_inline;
	if (!omni) return R_FALSE;
	return omni_key(omni, event->key);
}

static int on_input_event(Rid input, const REvent* event, void* user_data) {
	(void)input;
	/* the same keys, seen by the input itself (the window hook normally takes them first) */
	if (event->type != R_EVENT_KEY_DOWN) return R_FALSE;
	return omni_key((Omni*)user_data, event->key);
}

static int omni_key(Omni* omni, RKey key) {
	switch (key) {
	case R_KEY_UP: move_selection(omni, -1); return R_TRUE;
	case R_KEY_DOWN: move_selection(omni, 1); return R_TRUE;
	case R_KEY_ENTER:
	case R_KEY_KP_ENTER:
		run(omni);
		return R_TRUE;
	case R_KEY_ESCAPE:
		if (omni->palette) close_palette();
		else Rtext_input_set_text(Rwidget_find(g_window, omni->input), "");
		filter(omni, "");
		return R_TRUE;
	default: return R_FALSE;
	}
}

static void on_open(Rid sender, const char* action, void* user_data) {
	(void)sender;
	(void)action;
	(void)user_data;
	omnibar_toggle();
}

static void bind(Omni* omni) {
	Rid input = Rwidget_find(g_window, omni->input);
	Rid list = Rwidget_find(g_window, omni->list);
	if (input == R_INVALID_ID || list == R_INVALID_ID) return;
	Rtext_input_set_changed_callback(input, on_changed, omni);
	Rtext_input_set_committed_callback(input, on_committed, omni);
	Rwidget_set_event_callback(input, on_input_event, omni);
	roundlist_set_activate_callback(list, on_activated, omni);
	filter(omni, Rtext_input_get_text(input));
}

void omnibar_bind_widgets(void) {
	if (g_item_count == 0) build_items();
	bind(&g_palette);
	bind(&g_inline);
}

void omnibar_init(void) {
	Raction_bind("omni.open", on_open, NULL);
	app_add_event_hook(omni_hook);
}
