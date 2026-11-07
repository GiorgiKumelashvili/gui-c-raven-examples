/*
 * Simple HTTP: a method, a URL, headers that are added, edited, switched
 * off and deleted, a body; the JSON answer pretty-printed, its status, time
 * and headers. The request itself is native_http (NSURLSession).
 *
 * The headers are rows of the VBox "http_headers", each a <HeaderRow>
 * (widgets.xml): [check box][name][value][delete]. They are read back from
 * the widgets when the request leaves.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#define W(name) Rwidget_find(g_window, name)
#define MAX_HEADERS 32

typedef struct Preset {
	const char* label;
	const char* method;
	const char* url;
	const char* body;
} Preset;

static const Preset k_presets[] = {
	{ "GET a todo", "GET", "https://jsonplaceholder.typicode.com/todos/1", "" },
	{ "GET a list of users", "GET", "https://jsonplaceholder.typicode.com/users", "" },
	{ "POST a new post", "POST", "https://jsonplaceholder.typicode.com/posts",
		"{\n  \"title\": \"Hello from Raven\",\n  \"body\": \"Sent with NSURLSession\",\n  \"userId\": 1\n}" },
	{ "PUT a post", "PUT", "https://jsonplaceholder.typicode.com/posts/1",
		"{\n  \"id\": 1,\n  \"title\": \"Replaced\",\n  \"body\": \"Everything changes\",\n  \"userId\": 1\n}" },
	{ "PATCH a post", "PATCH", "https://jsonplaceholder.typicode.com/posts/1", "{\n  \"title\": \"Only the title\"\n}" },
	{ "DELETE a post", "DELETE", "https://jsonplaceholder.typicode.com/posts/1", "" },
	{ "Echo the headers (httpbin)", "GET", "https://httpbin.org/headers", "" },
	{ "An HTML page (refused: not JSON)", "GET", "https://example.com/", "" },
};
#define PRESET_COUNT ((int)(sizeof(k_presets) / sizeof(k_presets[0])))
static const char* const k_methods[] = { "GET", "POST", "PUT", "PATCH", "DELETE" };

static int g_busy;

static void add_header(const char* name, const char* value, int enabled) {
	static const char row_xml[] = "<HeaderRow/>";
	Rid list = W("http_headers");
	Rid row;
	if (list == R_INVALID_ID || Rwidget_get_child_count(list) >= MAX_HEADERS) return;
	row = Rwidget_create_from_xml(row_xml, sizeof(row_xml) - 1);
	if (row == R_INVALID_ID) return;
	Rwidget_add_child(list, row);
	Rcheckbox_set_checked(Rwidget_get_child(row, 0), enabled);
	Rtext_input_set_text(Rwidget_get_child(row, 1), name);
	Rtext_input_set_text(Rwidget_get_child(row, 2), value);
}

static const char* method_name(void) {
	Rid combo = W("http_method");
	int index = combo != R_INVALID_ID ? Rcombo_box_get_selected(combo) : 0;
	return k_methods[index >= 0 && index < 5 ? index : 0];
}

/* The body only means something for POST, PUT and PATCH. */
static void update_body(void) {
	const char* m = method_name();
	int has_body = strcmp(m, "GET") != 0 && strcmp(m, "DELETE") != 0;
	Rid body = W("http_req_body");
	if (body != R_INVALID_ID) Rwidget_set_enabled(body, has_body);
	if (W("http_body_note") != R_INVALID_ID) {
		Rlabel_set_text(W("http_body_note"), has_body ? "JSON sent as it is typed." : "No body for GET and DELETE.");
	}
}

static void apply_preset(int index) {
	int i;
	if (index < 0 || index >= PRESET_COUNT) return;
	for (i = 0; i < 5; i++) {
		if (strcmp(k_methods[i], k_presets[index].method) == 0) Rcombo_box_set_selected(W("http_method"), i);
	}
	Rtext_input_set_text(W("http_url"), k_presets[index].url);
	Rtext_editor_set_text(W("http_req_body"), k_presets[index].body);
	update_body();
}

static void set_status(const char* text, const char* color) {
	Rid label = W("http_status");
	if (label == R_INVALID_ID) return;
	Rlabel_set_text(label, text);
	Rwidget_set_attribute(label, "Color", color);
}

static void on_response(int status, double ms, const char* headers, const char* body, const char* error, void* user_data) {
	char text[96];
	(void)user_data;
	g_busy = 0;
	Rwidget_set_enabled(W("http_send"), R_TRUE);
	if (status > 0) {
		snprintf(text, sizeof(text), "%d %s", status,
			status < 300 ? "OK" : status < 400 ? "Redirect" : status < 500 ? "Client error" : "Server error");
		set_status(text, status < 300 ? "#30A46C" : status < 400 ? "#0090FF" : status < 500 ? "#F76B15" : "#E5484D");
	} else {
		set_status("Failed", "#E5484D");
	}
	snprintf(text, sizeof(text), "%.0f ms   -   %zu bytes", ms, body ? strlen(body) : 0);
	Rlabel_set_text(W("http_meta"), status > 0 || !error ? text : "");
	Rtext_editor_set_text(W("http_resp_headers"), headers ? headers : "");
	if (error) {
		Rtext_editor_set_language(W("http_resp_body"), "None");
		Rtext_editor_set_text(W("http_resp_body"), error);
	} else {
		Rtext_editor_set_language(W("http_resp_body"), "JSON");
		Rtext_editor_set_text(W("http_resp_body"), body && body[0] ? body : "(empty body)");
	}
}

static void send(void) {
	const char* names[MAX_HEADERS];
	const char* values[MAX_HEADERS];
	Rid list = W("http_headers");
	int i, n = 0;
	if (g_busy || list == R_INVALID_ID) return;
	for (i = 0; i < Rwidget_get_child_count(list) && n < MAX_HEADERS; i++) {
		Rid row = Rwidget_get_child(list, i);
		if (!Rcheckbox_is_checked(Rwidget_get_child(row, 0))) continue;
		names[n] = Rtext_input_get_text(Rwidget_get_child(row, 1));
		values[n] = Rtext_input_get_text(Rwidget_get_child(row, 2));
		if (names[n] && names[n][0]) n++;
	}
	g_busy = 1;
	Rwidget_set_enabled(W("http_send"), R_FALSE);
	set_status("Sending...", "#8B8D98");
	Rlabel_set_text(W("http_meta"), "");
	native_http(method_name(), Rtext_input_get_text(W("http_url")), names, values, n, Rtext_editor_get_text(W("http_req_body")),
		on_response, NULL);
}

static void on_action(Rid sender, const char* action, void* user_data) {
	(void)user_data;
	if (strcmp(action, "http.send") == 0) {
		send();
	} else if (strcmp(action, "http.header.add") == 0) {
		add_header("", "", R_TRUE);
	} else if (strcmp(action, "http.header.delete") == 0) {
		/* the button is the last child of its row */
		Rid row = Rwidget_get_parent(sender);
		if (row != R_INVALID_ID) Rwidget_destroy(row);
	} else if (strcmp(action, "http.preset") == 0) {
		apply_preset(Rcombo_box_get_selected(sender));
	} else if (strcmp(action, "http.method") == 0) {
		update_body();
	}
}

static void on_url_committed(Rid input, const char* text, void* user_data) {
	(void)input;
	(void)text;
	(void)user_data;
	send();
}

void http_init(void) {
	Raction_bind("http.send", on_action, NULL);
	Raction_bind("http.header.add", on_action, NULL);
	Raction_bind("http.header.delete", on_action, NULL);
	Raction_bind("http.preset", on_action, NULL);
	Raction_bind("http.method", on_action, NULL);
}

void http_bind_widgets(void) {
	Rid list = W("http_headers");
	Rid url = W("http_url");
	Rid presets = W("http_preset");
	int i;
	if (list == R_INVALID_ID) return;
	if (url != R_INVALID_ID) Rtext_input_set_committed_callback(url, on_url_committed, NULL);
	if (presets != R_INVALID_ID && Rcombo_box_get_item_count(presets) == 0) {
		for (i = 0; i < PRESET_COUNT; i++) Rcombo_box_add_item(presets, k_presets[i].label);
		Rcombo_box_set_selected(presets, 0);
	}
	if (Rwidget_get_child_count(list) == 0) {
		add_header("Accept", "application/json", R_TRUE);
		add_header("Content-Type", "application/json", R_TRUE);
		add_header("X-Client", "Raven Components", R_TRUE);
		add_header("Authorization", "Bearer <token>", R_FALSE);
		apply_preset(0);
	}
	update_body();
}
