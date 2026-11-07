/*
 * Virtual lists: 10 000 to 1 000 000 rows that are never stored.
 *
 * Raven's ListView and Table have a virtual mode: they ask for the text of
 * the rows they draw, nothing more (Rlist_view_set_virtual_items,
 * Rtable_set_virtual_rows). The rows here are computed from their number;
 * the app only keeps an array of indices: the rows the filter keeps, in the
 * order of the column the table is sorted by. Sorting compares numbers
 * (the names are indices into sorted arrays), never texts.
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

/* sorted, so that comparing their indices sorts the names */
static const char* const k_first[] = { "Ada", "Alan", "Barbara", "Claude", "Dennis", "Donald", "Edsger", "Frances", "Grace", "Guido",
	"Hedy", "Ivan", "John", "Ken", "Linus", "Margaret", "Niklaus", "Radia", "Richard", "Tim", "Yukihiro" };
static const char* const k_last[] = { "Allen", "Backus", "Dijkstra", "Hamilton", "Hopper", "Kay", "Knuth", "Lamarr", "Liskov",
	"Lovelace", "McCarthy", "Perlman", "Ritchie", "Shannon", "Stallman", "Sutherland", "Thompson", "Torvalds", "Turing", "Wirth" };
static const char* const k_city[] = { "Amsterdam", "Berlin", "Boston", "Cairo", "Kyoto", "Lagos", "Lima", "Lisbon", "London",
	"Melbourne", "Montreal", "Nairobi", "Oslo", "Paris", "Prague", "Seoul", "Tbilisi", "Tokyo", "Toronto", "Zurich" };
#define N_FIRST ((unsigned)(sizeof(k_first) / sizeof(k_first[0])))
#define N_LAST ((unsigned)(sizeof(k_last) / sizeof(k_last[0])))
#define N_CITY ((unsigned)(sizeof(k_city) / sizeof(k_city[0])))

static int* g_index;      /* the rows shown, in their order */
static int g_shown;       /* how many */
static int g_total = 100000;
static int g_sort_col = -1, g_sort_desc;
static char g_filter[128];
static double g_filter_ms, g_sort_ms;
static char g_text[160];

static unsigned hash(unsigned x) {
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

static unsigned first_of(int id) { return hash((unsigned)id * 3u + 1u) % N_FIRST; }
static unsigned last_of(int id) { return hash((unsigned)id * 7u + 2u) % N_LAST; }
static unsigned city_of(int id) { return hash((unsigned)id * 11u + 3u) % N_CITY; }
static unsigned score_of(int id) { return hash((unsigned)id * 13u + 5u) % 1000u; }

static const char* list_text(Rid list, int index, void* user_data) {
	int id;
	(void)list;
	(void)user_data;
	if (index < 0 || index >= g_shown) return "";
	id = g_index[index];
	snprintf(g_text, sizeof(g_text), "#%07d   %s %s, %s", id + 1, k_first[first_of(id)], k_last[last_of(id)], k_city[city_of(id)]);
	return g_text;
}

static const char* table_text(Rid table, int row, int column, void* user_data) {
	int id;
	(void)table;
	(void)user_data;
	if (row < 0 || row >= g_shown) return "";
	id = g_index[row];
	switch (column) {
	case 0: snprintf(g_text, sizeof(g_text), "%d", id + 1); break;
	case 1: snprintf(g_text, sizeof(g_text), "%s %s", k_first[first_of(id)], k_last[last_of(id)]); break;
	case 2: return k_city[city_of(id)];
	default: snprintf(g_text, sizeof(g_text), "%u.%u", score_of(id) / 10u, score_of(id) % 10u); break;
	}
	return g_text;
}

static unsigned sort_key(int id) {
	switch (g_sort_col) {
	case 1: return first_of(id) * N_LAST + last_of(id);
	case 2: return city_of(id);
	case 3: return score_of(id);
	default: return (unsigned)id;
	}
}

static int compare(const void* pa, const void* pb) {
	int a = *(const int*)pa, b = *(const int*)pb;
	unsigned ka = sort_key(a), kb = sort_key(b);
	int result = ka < kb ? -1 : ka > kb ? 1 : (a < b ? -1 : a > b);
	return g_sort_desc ? -result : result;
}

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

static void show_stats(void) {
	char text[200];
	Rid label = Rwidget_find(g_window, "vl_stats");
	snprintf(text, sizeof(text), "%d of %d rows   -   filter %.1f ms   -   sort %.1f ms", g_shown, g_total, g_filter_ms, g_sort_ms);
	if (label != R_INVALID_ID) Rlabel_set_text(label, text);
}

static void publish(void) {
	Rid list = Rwidget_find(g_window, "vl_list");
	Rid table = Rwidget_find(g_window, "vl_table");
	if (list != R_INVALID_ID) Rlist_view_set_virtual_items(list, g_shown, list_text, NULL);
	if (table != R_INVALID_ID) Rtable_set_virtual_rows(table, g_shown, table_text, NULL);
	show_stats();
}

static void sort_rows(void) {
	double start = Rget_time();
	if (g_sort_col >= 0) qsort(g_index, (size_t)g_shown, sizeof(int), compare);
	g_sort_ms = (Rget_time() - start) * 1000.0;
}

/*
 * The filter: every word must be found in the name, the city or the number.
 * The names and the cities are a few dozen texts: each word is tried on them
 * once, and the million rows only look up the answers.
 */
#define MAX_WORDS 8

static void rebuild(void) {
	char words[MAX_WORDS][32], number[16];
	unsigned char hit_first[MAX_WORDS][N_FIRST], hit_last[MAX_WORDS][N_LAST], hit_city[MAX_WORDS][N_CITY];
	int digits[MAX_WORDS], word_count = 0, w, id;
	const char* cursor = g_filter;
	double start = Rget_time();
	unsigned k;

	while (*cursor && word_count < MAX_WORDS) {
		size_t length;
		while (*cursor == ' ') cursor++;
		length = strcspn(cursor, " ");
		if (length == 0) break;
		if (length > 31) length = 31;
		memcpy(words[word_count], cursor, length);
		words[word_count][length] = '\0';
		cursor += strcspn(cursor, " ");
		digits[word_count] = strspn(words[word_count], "0123456789") == length;
		for (k = 0; k < N_FIRST; k++) hit_first[word_count][k] = (unsigned char)contains_nocase(k_first[k], words[word_count]);
		for (k = 0; k < N_LAST; k++) hit_last[word_count][k] = (unsigned char)contains_nocase(k_last[k], words[word_count]);
		for (k = 0; k < N_CITY; k++) hit_city[word_count][k] = (unsigned char)contains_nocase(k_city[k], words[word_count]);
		word_count++;
	}

	g_index = (int*)realloc(g_index, sizeof(int) * (size_t)g_total);
	g_shown = 0;
	for (id = 0; id < g_total; id++) {
		unsigned f = first_of(id), l = last_of(id), c = city_of(id);
		for (w = 0; w < word_count; w++) {
			if (hit_first[w][f] || hit_last[w][l] || hit_city[w][c]) continue;
			if (digits[w]) {
				snprintf(number, sizeof(number), "%d", id + 1);
				if (strstr(number, words[w])) continue;
			}
			break;
		}
		if (w == word_count) g_index[g_shown++] = id;
	}
	g_filter_ms = (Rget_time() - start) * 1000.0;
	sort_rows();
	publish();
}

static void on_filter(Rid input, const char* text, void* user_data) {
	(void)input;
	(void)user_data;
	snprintf(g_filter, sizeof(g_filter), "%s", text ? text : "");
	rebuild();
}

static void on_sort(Rid table, int column, int descending, void* user_data) {
	(void)table;
	(void)user_data;
	g_sort_col = column;
	g_sort_desc = descending;
	if (column < 0) {
		/* back to the order of the numbers */
		rebuild();
		return;
	}
	sort_rows();
	publish();
}

static void on_action(Rid sender, const char* action, void* user_data) {
	static const int counts[] = { 10000, 100000, 1000000 };
	(void)user_data;
	if (strcmp(action, "vl.count") == 0) {
		int selected = Rcombo_box_get_selected(sender);
		g_total = counts[selected >= 0 && selected < 3 ? selected : 1];
		rebuild();
	} else if (strcmp(action, "vl.jump") == 0 && g_shown > 0) {
		int row = (int)(hash((unsigned)(Rget_time() * 1000.0)) % (unsigned)g_shown);
		Rid list = Rwidget_find(g_window, "vl_list");
		Rid table = Rwidget_find(g_window, "vl_table");
		Rlist_view_set_selected(list, row);
		Rlist_view_scroll_to_item(list, row);
		Rtable_set_selected(table, row);
		Rtable_scroll_to_row(table, row);
	}
}

void vlist_init(void) {
	Raction_bind("vl.count", on_action, NULL);
	Raction_bind("vl.jump", on_action, NULL);
}

void vlist_bind_widgets(void) {
	Rid filter = Rwidget_find(g_window, "vl_filter");
	Rid table = Rwidget_find(g_window, "vl_table");
	if (filter == R_INVALID_ID || table == R_INVALID_ID) return;
	Rtext_input_set_changed_callback(filter, on_filter, NULL);
	Rtable_set_sort_callback(table, on_sort, NULL);
	rebuild();
}
