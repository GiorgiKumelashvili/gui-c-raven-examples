/*
 * The sidebar: a tree of sections and the pages they hold, filtered by the
 * text typed above it. Choosing a page shows it in the Switcher "pages" and
 * writes where the user is in the breadcrumb of the content header.
 *
 * Each page is a file of src/pages/ included by main.xml; its root is named
 * "page.<id>" so that it is found by name, whatever its position.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "app.h"

typedef struct NavEntry {
	const char* id;
	const char* title;
} NavEntry;

typedef struct NavSection {
	const char* title;
	const char* icon;
	const NavEntry* entries;
	int count;
} NavSection;

#define ENTRIES(name) name, (int)(sizeof(name) / sizeof(name[0]))

static const NavEntry k_start[] = { { "home", "Welcome" }, { "themes", "Themes & looks" } };
static const NavEntry k_layout[] = {
	{ "layout.boxes", "Boxes & grids" }, { "layout.splitter", "Splitter" }, { "layout.tabs", "Tabs" },
	{ "layout.collapse", "Collapse" }, { "layout.dock", "Dock area" }, { "layout.surfaces", "Surfaces & effects" },
};
static const NavEntry k_inputs[] = {
	{ "input.buttons", "Buttons" }, { "input.checks", "Checks & switches" }, { "input.text", "Text fields" },
	{ "input.numbers", "Numbers & sliders" }, { "input.choice", "Choice & lists" }, { "input.color", "Color picker" },
};
static const NavEntry k_navigation[] = {
	{ "nav.menubar", "Menu bar" }, { "nav.context", "Context menu" }, { "nav.breadcrumbs", "Breadcrumbs" },
	{ "nav.pagination", "Pagination" },
};
static const NavEntry k_data[] = { { "data.tree", "Tree" }, { "data.table", "Table" } };
static const NavEntry k_time[] = { { "time.date", "Date picker" }, { "time.time", "Time picker" } };
static const NavEntry k_overlays[] = {
	{ "overlay.alert", "Alert" }, { "overlay.modal", "Modal" }, { "overlay.toast", "Toast" },
	{ "overlay.tooltip", "Tooltip" }, { "overlay.progress", "Progress" },
};
static const NavEntry k_text[] = { { "text.markdown", "Typography & Markdown" }, { "text.editor", "Code editor" } };
static const NavEntry k_tools[] = { { "tools.nodes", "Node graph" }, { "tools.timeline", "Timeline" } };
static const NavEntry k_custom[] = {
	{ "custom.nativemenu", "macOS context menu" }, { "custom.finder", "Finder file panels" },
	{ "custom.notify", "macOS notifications" }, { "custom.datatable", "Data table" },
	{ "custom.buttongroup", "Button group" }, { "custom.segmented", "Segmented control" },
	{ "custom.popover", "Popover" }, { "custom.omnibar", "Omni bar" },
	{ "custom.fileinput", "File input" }, { "custom.textarea", "Plain textarea" },
	{ "custom.virtual", "Virtual list (1M rows)" }, { "custom.canvas", "Canvas" },
	{ "custom.accordion", "Accordion" }, { "custom.http", "Simple HTTP" },
	{ "custom.scrollspeed", "Scroll speeds" }, { "custom.multiwindow", "Windows & shared state" },
};

static const NavSection k_sections[] = {
	{ "Getting started", "home", ENTRIES(k_start) },
	{ "Layout", "layout", ENTRIES(k_layout) },
	{ "Buttons & inputs", "input", ENTRIES(k_inputs) },
	{ "Navigation", "compass", ENTRIES(k_navigation) },
	{ "Data", "table", ENTRIES(k_data) },
	{ "Date & time", "calendar", ENTRIES(k_time) },
	{ "Overlays & feedback", "bell", ENTRIES(k_overlays) },
	{ "Text & code", "text", ENTRIES(k_text) },
	{ "Tools", "wand", ENTRIES(k_tools) },
	{ "Custom (not in Raven)", "puzzle", ENTRIES(k_custom) },
};
#define SECTION_COUNT ((int)(sizeof(k_sections) / sizeof(k_sections[0])))

static Rid g_icons[SECTION_COUNT];
static const char* g_current = "home";

/* history, as in a browser: back and forward */
#define HISTORY_MAX 64
static const char* g_back[HISTORY_MAX];
static int g_back_count;
static const char* g_forward[HISTORY_MAX];
static int g_forward_count;
static int g_in_history;
static int g_rebuilding;
static int g_laid_out;

static int contains_nocase(const char* text, const char* part) {
	size_t i, j, n = strlen(part);
	if (n == 0) return 1;
	for (i = 0; text[i]; i++) {
		for (j = 0; j < n && text[i + j]; j++) {
			if (tolower((unsigned char)text[i + j]) != tolower((unsigned char)part[j])) break;
		}
		if (j == n) return 1;
	}
	return 0;
}

static const NavEntry* find_entry(const char* id, const NavSection** section) {
	int s, e;
	for (s = 0; s < SECTION_COUNT; s++) {
		for (e = 0; e < k_sections[s].count; e++) {
			if (strcmp(k_sections[s].entries[e].id, id) == 0) {
				if (section) *section = &k_sections[s];
				return &k_sections[s].entries[e];
			}
		}
	}
	return NULL;
}

int nav_entry_count(void) {
	int s, n = 0;
	for (s = 0; s < SECTION_COUNT; s++) n += k_sections[s].count;
	return n;
}

static const NavEntry* entry_at(int index, const NavSection** section) {
	int s;
	for (s = 0; s < SECTION_COUNT; s++) {
		if (index < k_sections[s].count) {
			if (section) *section = &k_sections[s];
			return &k_sections[s].entries[index];
		}
		index -= k_sections[s].count;
	}
	return NULL;
}

const char* nav_entry_id(int index) {
	const NavEntry* entry = entry_at(index, NULL);
	return entry ? entry->id : "";
}

const char* nav_entry_title(int index) {
	const NavEntry* entry = entry_at(index, NULL);
	return entry ? entry->title : "";
}

const char* nav_entry_section(int index) {
	const NavSection* section = NULL;
	return entry_at(index, &section) ? section->title : "";
}

static void push(const char** stack, int* count, const char* id) {
	if (*count == HISTORY_MAX) {
		memmove(stack, stack + 1, sizeof(char*) * (HISTORY_MAX - 1));
		(*count)--;
	}
	stack[(*count)++] = id;
}

static void update_arrows(void) {
	Rid back = Rwidget_find(g_window, "nav_back");
	Rid forward = Rwidget_find(g_window, "nav_forward");
	if (back != R_INVALID_ID) Rwidget_set_enabled(back, g_back_count > 0);
	if (forward != R_INVALID_ID) Rwidget_set_enabled(forward, g_forward_count > 0);
}

void nav_back(void) {
	if (g_back_count == 0) return;
	push(g_forward, &g_forward_count, g_current);
	g_in_history = 1;
	nav_go(g_back[--g_back_count]);
	g_in_history = 0;
	update_arrows();
}

void nav_forward(void) {
	if (g_forward_count == 0) return;
	push(g_back, &g_back_count, g_current);
	g_in_history = 1;
	nav_go(g_forward[--g_forward_count]);
	g_in_history = 0;
	update_arrows();
}

static Rid find_tree_item(Rid tree, const char* id) {
	int s, e;
	for (s = 0; s < Rtree_view_get_item_count(tree); s++) {
		Rid section = Rtree_view_get_item(tree, s);
		for (e = 0; e < Rtree_item_get_child_count(section); e++) {
			Rid item = Rtree_item_get_child(section, e);
			const NavEntry* entry = (const NavEntry*)Rtree_item_get_user_handle(item);
			if (entry && strcmp(entry->id, id) == 0) return item;
		}
	}
	return R_INVALID_ID;
}

void nav_go(const char* id) {
	char name[64], crumbs[160];
	const NavSection* section = NULL;
	const NavEntry* entry = find_entry(id, &section);
	Rid pages = Rwidget_find(g_window, "pages");
	Rid tree = Rwidget_find(g_window, "nav");
	Rid page;
	int i;

	if (!entry || pages == R_INVALID_ID) return;
	/* a new place: the one left goes to the history, the way forward is forgotten */
	if (!g_in_history && strcmp(entry->id, g_current) != 0 && g_laid_out) {
		push(g_back, &g_back_count, g_current);
		g_forward_count = 0;
	}
	g_current = entry->id;
	popover_close();

	snprintf(name, sizeof(name), "page.%s", entry->id);
	page = Rwidget_find(pages, name);
	for (i = 0; i < Rwidget_get_child_count(pages); i++) {
		if (Rwidget_get_child(pages, i) == page) {
			Rswitcher_set_index(pages, i);
			break;
		}
	}
	snprintf(crumbs, sizeof(crumbs), "Components|%s|%s", section->title, entry->title);
	Rbreadcrumb_set_items(Rwidget_find(g_window, "crumbs"), crumbs);

	update_arrows();
	if (tree != R_INVALID_ID && !g_rebuilding) {
		Rid item = find_tree_item(tree, entry->id);
		if (item != R_INVALID_ID && Rtree_view_get_selected_item(tree) != item) {
			g_rebuilding = 1;
			Rtree_view_set_selected_item(tree, item);
			/* before the first layout the tree has no size to scroll in */
			if (g_laid_out) Rtree_view_scroll_to_item(tree, item);
			g_rebuilding = 0;
		}
	}
}

static void on_tree_selection(Rid tree, Rid item, void* user_data) {
	const NavEntry* entry;
	(void)tree;
	(void)user_data;
	if (g_rebuilding || item == R_INVALID_ID) return;
	entry = (const NavEntry*)Rtree_item_get_user_handle(item);
	if (entry) nav_go(entry->id);
}

/* A section row: a click folds and unfolds it, as in the sidebar of Finder. */
static void on_tree_activated(Rid tree, Rid item, void* user_data) {
	(void)tree;
	(void)user_data;
	if (item != R_INVALID_ID && !Rtree_item_get_user_handle(item)) Rtree_item_set_expanded(item, !Rtree_item_is_expanded(item));
}

void nav_rebuild(const char* filter) {
	Rid tree = Rwidget_find(g_window, "nav");
	int s, e;
	if (tree == R_INVALID_ID) return;
	g_rebuilding = 1;
	Rtree_view_clear_items(tree);
	for (s = 0; s < SECTION_COUNT; s++) {
		const NavSection* section = &k_sections[s];
		Rid parent = R_INVALID_ID;
		int section_matches = filter && contains_nocase(section->title, filter);
		for (e = 0; e < section->count; e++) {
			const NavEntry* entry = &section->entries[e];
			Rid item;
			if (filter && filter[0] && !section_matches && !contains_nocase(entry->title, filter)) continue;
			if (parent == R_INVALID_ID) {
				parent = Rtree_view_add_item(tree, R_INVALID_ID, section->title);
				if (g_icons[s] != R_INVALID_ID) Rtree_item_set_icon(parent, g_icons[s], 0.0f, 0.0f, 1.0f, 1.0f);
				Rtree_item_set_expanded(parent, R_TRUE);
			}
			item = Rtree_view_add_item(tree, parent, entry->title);
			Rtree_item_set_user_handle(item, (void*)entry);
		}
	}
	{
		Rid item = find_tree_item(tree, g_current);
		if (item != R_INVALID_ID) Rtree_view_set_selected_item(tree, item);
	}
	g_rebuilding = 0;
}

static void on_filter_changed(Rid input, const char* text, void* user_data) {
	(void)input;
	(void)user_data;
	nav_rebuild(text);
}

static void on_crumb(Rid breadcrumb, int index, void* user_data) {
	(void)breadcrumb;
	(void)user_data;
	/* "Components" goes home; a section goes to its first page */
	if (index == 0) nav_go("home");
	else if (index == 1) {
		const NavSection* section = NULL;
		if (find_entry(g_current, &section)) nav_go(section->entries[0].id);
	}
}

void nav_init(void) {
	char path[512];
	int s;
	for (s = 0; s < SECTION_COUNT; s++) {
		if (g_icons[s] != R_INVALID_ID && g_icons[s] != 0) continue;
		snprintf(path, sizeof(path), "%s/icons/%s.svg", APP_UI_DIR, k_sections[s].icon);
		g_icons[s] = Rtexture_create_from_file(path);
	}
	Rtree_view_set_selection_changed_callback(Rwidget_find(g_window, "nav"), on_tree_selection, NULL);
	Rtree_view_set_activated_callback(Rwidget_find(g_window, "nav"), on_tree_activated, NULL);
	Rtext_input_set_changed_callback(Rwidget_find(g_window, "nav_filter"), on_filter_changed, NULL);
	Rbreadcrumb_set_clicked_callback(Rwidget_find(g_window, "crumbs"), on_crumb, NULL);
	nav_rebuild(Rtext_input_get_text(Rwidget_find(g_window, "nav_filter")));
	nav_go(g_current);
	g_laid_out = 1;
}
