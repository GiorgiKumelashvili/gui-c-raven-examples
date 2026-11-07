/*
 * The actions of the demo pages.
 *
 * Most pages need no C: their XML uses actions of Raven itself
 * (toggle:NAME, page:NAME=+1, theme:NAME, copy:TEXT...). What remains is
 * here. A few families of names are read by the default handler:
 *
 *   say:TEXT            shows TEXT in the toast
 *   popover:CONTENT     opens the popover with the content named CONTENT
 *   dt.COMMAND          a command of the data table (datatable_command)
 *   file.browse[:KIND]  the file dialog of the system, for a FileInput
 *   menu.*              an item of the menu bar: says which
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

#define W(name) Rwidget_find(g_window, name)

static void set_label(const char* name, const char* text) {
	Rid label = W(name);
	if (label != R_INVALID_ID) Rlabel_set_text(label, text);
}

static int starts_with(const char* text, const char* prefix) {
	return strncmp(text, prefix, strlen(prefix)) == 0;
}

/* The root of what contains a widget: a dialog content, a popup, or the window content. */
static Rid root_of(Rid widget) {
	Rid parent;
	while ((parent = Rwidget_get_parent(widget)) != R_INVALID_ID) widget = parent;
	return widget;
}

/* ---------------------------------------------------------------------------
 *  Overlays: alert, modal, toast, progress
 * ------------------------------------------------------------------------- */

static void on_alert(Rid sender, const char* action, void* user_data) {
	RMessageResult result;
	const char* answer = "dismissed";
	(void)sender;
	(void)user_data;
	if (strcmp(action, "alert.info") == 0) {
		Rmessage_box(g_window, "Update installed", "Raven Components was updated to version 1.4. Restart to apply.", R_MESSAGE_OK);
		return;
	}
	if (strcmp(action, "alert.confirm") == 0) {
		result = Rmessage_box(g_window, "Move to Trash?", "The file \"report-q3.pdf\" will be moved to the Trash.", R_MESSAGE_OK_CANCEL);
	} else if (strcmp(action, "alert.danger") == 0) {
		result = Rmessage_box(g_window, "Delete the project?", "This cannot be undone. 128 files and their history will be removed.", R_MESSAGE_YES_NO);
	} else {
		result = Rmessage_box(g_window, "Save changes?", "Do you want to save the changes made to \"untitled.xml\"?", R_MESSAGE_YES_NO_CANCEL);
	}
	switch (result) {
	case R_MESSAGE_RESULT_OK: answer = "OK"; break;
	case R_MESSAGE_RESULT_YES: answer = "Yes"; break;
	case R_MESSAGE_RESULT_NO: answer = "No"; break;
	default: answer = "Cancel"; break;
	}
	{
		char text[96];
		snprintf(text, sizeof(text), "The alert answered: %s", answer);
		set_label("alert_result", text);
		app_toast(text, result == R_MESSAGE_RESULT_CANCEL ? R_TOAST_WARNING : R_TOAST_SUCCESS);
	}
}

static void on_modal_closed(Rid content, int result, void* user_data) {
	(void)content;
	(void)user_data;
	if (result == 0) set_label("modal_result", "The modal was cancelled (Escape or Cancel).");
}

static void on_modal(Rid sender, const char* action, void* user_data) {
	(void)user_data;
	if (strcmp(action, "modal.open") == 0 || strcmp(action, "modal.open.form") == 0) {
		Rid content = Rwidget_create_from_xml_file(APP_UI_DIR "/dialogs/invite.xml");
		if (content != R_INVALID_ID) Rdialog_open(g_window, content, on_modal_closed, NULL);
	} else if (strcmp(action, "modal.open.terms") == 0) {
		Rid content = Rwidget_create_from_xml_file(APP_UI_DIR "/dialogs/terms.xml");
		if (content != R_INVALID_ID) Rdialog_open(g_window, content, on_modal_closed, NULL);
	} else if (strcmp(action, "modal.ok") == 0) {
		/* read the form before the dialog closes: its widgets are deleted with it */
		Rid root = root_of(sender);
		Rid email = Rwidget_find(root, "invite_email");
		Rid role = Rwidget_find(root, "invite_role");
		char text[200];
		snprintf(text, sizeof(text), "Invited %s as %s.",
			email != R_INVALID_ID && Rtext_input_get_text(email)[0] ? Rtext_input_get_text(email) : "nobody",
			role != R_INVALID_ID ? Rcombo_box_get_item_text(role, Rcombo_box_get_selected(role)) : "?");
		set_label("modal_result", text);
		app_toast(text, R_TOAST_SUCCESS);
		Rdialog_close(sender, 1);
	} else if (strcmp(action, "modal.accept") == 0) {
		set_label("modal_result", "Terms accepted.");
		Rdialog_close(sender, 1);
	} else if (strcmp(action, "modal.cancel") == 0) {
		Rdialog_close(sender, 0);
	}
}

static void on_toast_undo(Rid toast, void* user_data) {
	(void)toast;
	(void)user_data;
	app_toast("Restored \"Q3 planning\".", R_TOAST_SUCCESS);
}

static void on_toast(Rid sender, const char* action, void* user_data) {
	Rid toast = W("toast");
	static int count;
	char text[64];
	(void)sender;
	(void)user_data;
	if (toast == R_INVALID_ID) return;
	Rtoast_set_action_text(toast, NULL);
	if (strcmp(action, "toast.info") == 0) Rtoast_show_kind(toast, "Sync started in the background.", R_TOAST_INFO, 3.0);
	else if (strcmp(action, "toast.success") == 0) Rtoast_show_kind(toast, "Changes saved.", R_TOAST_SUCCESS, 3.0);
	else if (strcmp(action, "toast.warning") == 0) Rtoast_show_kind(toast, "Battery low: 9% left.", R_TOAST_WARNING, 3.0);
	else if (strcmp(action, "toast.error") == 0) Rtoast_show_kind(toast, "Upload failed: connection lost.", R_TOAST_ERROR, 4.0);
	else if (strcmp(action, "toast.action") == 0) {
		Rtoast_set_action_text(toast, "Undo");
		Rtoast_set_action_callback(toast, on_toast_undo, NULL);
		Rtoast_show_kind(toast, "Moved \"Q3 planning\" to the Trash.", R_TOAST_INFO, 5.0);
		Rtoast_set_action_text(toast, NULL);
	} else if (strcmp(action, "toast.burst") == 0) {
		int i;
		for (i = 0; i < 3; i++) {
			snprintf(text, sizeof(text), "Message %d of a burst", ++count);
			Rtoast_show_kind(toast, text, (RToastKind)(R_TOAST_INFO + i), 3.0);
		}
	} else if (strcmp(action, "toast.queue") == 0) {
		Rid combo = W("toast_queue");
		int selected = combo != R_INVALID_ID ? Rcombo_box_get_selected(combo) : 0;
		Rtoast_set_queue(toast, selected == 1 ? R_TOAST_SEQUENCE : selected == 2 ? R_TOAST_STACK : R_TOAST_REPLACE);
	} else if (strcmp(action, "toast.clear") == 0) {
		Rtoast_clear(toast);
	}
}

static float g_progress;
static int g_progress_running;

static void on_frame(double delta, void* user_data) {
	char text[48];
	(void)user_data;
	if (!g_progress_running) return;
	g_progress += (float)delta * 0.25f;
	if (g_progress >= 1.0f) {
		g_progress = 1.0f;
		g_progress_running = 0;
		Rwindow_set_continuous_redraw(g_window, R_FALSE);
		app_toast("Export finished.", R_TOAST_SUCCESS);
	}
	Rprogress_bar_set_value(W("progress_main"), g_progress);
	Rprogress_bar_set_value(W("progress_small"), g_progress * g_progress);
	snprintf(text, sizeof(text), "Exporting... %d%%", (int)(g_progress * 100.0f));
	set_label("progress_label", g_progress >= 1.0f ? "Done." : text);
}

static void on_progress(Rid sender, const char* action, void* user_data) {
	(void)sender;
	(void)user_data;
	if (strcmp(action, "progress.start") == 0) {
		if (g_progress >= 1.0f) g_progress = 0.0f;
		g_progress_running = 1;
		Rwindow_set_continuous_redraw(g_window, R_TRUE);
	} else if (strcmp(action, "progress.pause") == 0) {
		g_progress_running = 0;
		Rwindow_set_continuous_redraw(g_window, R_FALSE);
	} else {
		g_progress_running = 0;
		g_progress = 0.0f;
		Rwindow_set_continuous_redraw(g_window, R_FALSE);
		Rprogress_bar_set_value(W("progress_main"), 0.0f);
		Rprogress_bar_set_value(W("progress_small"), 0.0f);
		set_label("progress_label", "Ready.");
	}
}

/* ---------------------------------------------------------------------------
 *  Context menus: Raven's and the system's
 * ------------------------------------------------------------------------- */

static Rid g_raven_menu = R_INVALID_ID;
static Rid g_tree_menu = R_INVALID_ID;

static const char k_raven_menu_xml[] =
	"<Menu>"
	"  <MenuItem Text=\"Cut\" Shortcut=\"Super+X\" OnClicked=\"ctx.item\"/>"
	"  <MenuItem Text=\"Copy\" Shortcut=\"Super+C\" OnClicked=\"ctx.item\"/>"
	"  <MenuItem Text=\"Paste\" Shortcut=\"Super+V\" OnClicked=\"ctx.item\"/>"
	"  <Separator/>"
	"  <MenuItem Text=\"Show grid\" Checkable=\"true\" Checked=\"true\" OnClicked=\"ctx.item\"/>"
	"  <MenuItem Text=\"Snap to pixels\" Checkable=\"true\" OnClicked=\"ctx.item\"/>"
	"  <Separator/>"
	"  <Menu Text=\"Arrange\">"
	"    <MenuItem Text=\"Bring to front\" OnClicked=\"ctx.item\"/>"
	"    <MenuItem Text=\"Send to back\" OnClicked=\"ctx.item\"/>"
	"    <Menu Text=\"Align\">"
	"      <MenuItem Text=\"Left\" OnClicked=\"ctx.item\"/>"
	"      <MenuItem Text=\"Center\" OnClicked=\"ctx.item\"/>"
	"      <MenuItem Text=\"Right\" OnClicked=\"ctx.item\"/>"
	"    </Menu>"
	"  </Menu>"
	"  <Separator/>"
	"  <MenuItem Text=\"Delete\" Shortcut=\"Delete\" OnClicked=\"ctx.item\"/>"
	"</Menu>";

static const char k_tree_menu_xml[] =
	"<Menu>"
	"  <MenuItem Text=\"Open\" OnClicked=\"ctx.item\"/>"
	"  <MenuItem Text=\"Rename\" OnClicked=\"ctx.item\"/>"
	"  <MenuItem Text=\"Duplicate\" OnClicked=\"ctx.item\"/>"
	"  <Separator/>"
	"  <MenuItem Text=\"Move to Trash\" OnClicked=\"ctx.item\"/>"
	"</Menu>";

static void on_ctx_item(Rid sender, const char* action, void* user_data) {
	char text[96];
	(void)action;
	(void)user_data;
	snprintf(text, sizeof(text), "Raven menu: %s%s", Rmenu_item_get_text(sender),
		Rmenu_item_is_checked(sender) ? " (checked)" : "");
	set_label("ctx_result", text);
	app_toast(text, R_TOAST_INFO);
}

static void on_ctx_button(Rid sender, const char* action, void* user_data) {
	float x, y, w, h;
	(void)action;
	(void)user_data;
	Rwidget_get_rect(sender, &x, &y, &w, &h);
	Rmenu_open(g_raven_menu, g_window, x, y + h + 4.0f);
}

static void on_tree_context(Rid tree, Rid item, float x, float y, void* user_data) {
	(void)user_data;
	if (item != R_INVALID_ID) Rtree_view_set_selected_item(tree, item);
	Rmenu_open(g_tree_menu, g_window, x, y);
}

static const char* const k_native_items[] = {
	"doc.on.doc|Copy", "doc.on.clipboard|Paste", "pencil|Rename", "-",
	"square.and.arrow.up|Share...", "tag|Add tag", "star|Add to favorites", "-",
	"info.circle|Get info", "trash|Move to Trash"
};
#define NATIVE_COUNT ((int)(sizeof(k_native_items) / sizeof(k_native_items[0])))

static void native_menu_at(float x, float y) {
	char text[96];
	int chosen = native_context_menu(g_window, x, y, k_native_items, NATIVE_COUNT);
	if (chosen == -2) {
		/* not on macOS: Raven's menu instead */
		Rmenu_open(g_raven_menu, g_window, x, y);
		set_label("native_result", "No native menu on this system: Raven's menu was shown.");
		return;
	}
	if (chosen < 0) {
		set_label("native_result", "Dismissed.");
		return;
	}
	snprintf(text, sizeof(text), "macOS menu: %s", strchr(k_native_items[chosen], '|') + 1);
	set_label("native_result", text);
	app_toast(text, R_TOAST_INFO);
}

static int on_native_area_event(Rid widget, const REvent* event, void* user_data) {
	(void)widget;
	(void)user_data;
	/* opened when the right button is released, as Raven's menus are:
	 * AppKit tracks the menu in its own loop and Raven never sees that release */
	if (event->type == R_EVENT_MOUSE_UP && event->button == R_MOUSE_RIGHT) {
		native_menu_at(event->x, event->y);
		return R_TRUE;
	}
	if (event->type == R_EVENT_MOUSE_DOWN && event->button == R_MOUSE_RIGHT) return R_TRUE;
	return R_FALSE;
}

static void on_native_button(Rid sender, const char* action, void* user_data) {
	float x, y, w, h;
	(void)action;
	(void)user_data;
	Rwidget_get_rect(sender, &x, &y, &w, &h);
	native_menu_at(x, y + h + 4.0f);
}

/* ---------------------------------------------------------------------------
 *  Custom components
 * ------------------------------------------------------------------------- */

static void on_dt_changed(Rid sender, const char* action, void* user_data) {
	char text[64];
	(void)action;
	(void)user_data;
	snprintf(text, sizeof(text), "%d of %d rows selected", datatable_checked_count(sender), datatable_row_count(sender));
	set_label("dt_status", text);
}

static void on_segmented(Rid sender, const char* action, void* user_data) {
	char text[96];
	int selected = segmented_get_selected(sender);
	(void)user_data;
	if (strcmp(action, "seg.view") == 0) {
		Rid views = W("seg_views");
		if (views != R_INVALID_ID) Rswitcher_set_index(views, selected);
	}
	snprintf(text, sizeof(text), "Selected: %s (index %d)", segmented_get_item(sender, selected), selected);
	set_label("seg_status", text);
}

/* Raven has no getter for the text of a button: read it from the XML it was built from. */
static const char* button_text(Rid button, int index) {
	static char fallback[32];
	Rid node = Rwidget_get_xml_node(button);
	const char* text = node != R_INVALID_ID ? Rxml_node_get_attribute(node, "Text") : NULL;
	if (text && text[0]) return text;
	snprintf(fallback, sizeof(fallback), "button %d", index + 1);
	return fallback;
}

/* Exclusive group: the clicked button becomes the primary one of its group. */
static void on_group_select(Rid sender, const char* action, void* user_data) {
	Rid group = Rwidget_get_parent(sender);
	char text[96];
	int i;
	(void)action;
	(void)user_data;
	int index = 0;
	for (i = 0; i < Rwidget_get_child_count(group); i++) {
		Rid child = Rwidget_get_child(group, i);
		if (Rwidget_get_type(child) != R_WIDGET_BUTTON) continue;
		if (child == sender) index = i;
		Rbutton_set_style(child, child == sender ? R_BUTTON_PRIMARY : R_BUTTON_FLAT);
	}
	snprintf(text, sizeof(text), "Chosen: %s", button_text(sender, index));
	set_label("bg_status", text);
}

/* Toggle group: each button is on or off on its own. */
static void on_group_toggle(Rid sender, const char* action, void* user_data) {
	int on = Rwidget_has_class(sender, "on");
	(void)action;
	(void)user_data;
	if (on) Rwidget_remove_class(sender, "on");
	else Rwidget_add_class(sender, "on");
	Rbutton_set_style(sender, on ? R_BUTTON_FLAT : R_BUTTON_PRIMARY);
}

static void browse(Rid sender, const char* kind) {
	static const RFileFilter images[] = { { "Images", "*.png;*.jpg;*.jpeg;*.gif;*.svg" } };
	static const RFileFilter documents[] = { { "Documents", "*.pdf;*.txt;*.md;*.xml;*.json" } };
	RFileDialogDesc desc;
	const char* path;
	Rid field = Rwidget_get_parent(sender);
	Rid label = field != R_INVALID_ID ? Rwidget_get_child(field, 0) : R_INVALID_ID;
	char text[512];

	memset(&desc, 0, sizeof(desc));
	desc.parent = g_window;
	if (kind && strcmp(kind, "images") == 0) {
		desc.filters = images;
		desc.filter_count = 1;
		desc.title = "Choose an image";
	} else if (kind && strcmp(kind, "documents") == 0) {
		desc.filters = documents;
		desc.filter_count = 1;
	} else if (kind && strcmp(kind, "multi") == 0) {
		desc.flags = R_FILE_DIALOG_MULTIPLE;
	} else if (kind && strcmp(kind, "raven") == 0) {
		desc.flags = R_FILE_DIALOG_RAVEN;
	}
	path = kind && strcmp(kind, "folder") == 0 ? Rfile_dialog_select_folder(&desc) : Rfile_dialog_open(&desc);
	if (!path) return;
	if (Rfile_dialog_get_result_count() > 1) {
		snprintf(text, sizeof(text), "%d files chosen", Rfile_dialog_get_result_count());
	} else {
		const char* slash = strrchr(path, '/');
		snprintf(text, sizeof(text), "%s", slash ? slash + 1 : path);
	}
	if (label != R_INVALID_ID && Rwidget_get_type(label) == R_WIDGET_LABEL) {
		Rlabel_set_text(label, text);
		Rwidget_set_attribute(label, "Opacity", "1");
	}
	snprintf(text, sizeof(text), "%s", path);
	set_label("file_path", text);
}

/* ---------------------------------------------------------------------------
 *  Small things of the other pages
 * ------------------------------------------------------------------------- */

static void on_value(Rid sender, const char* action, void* user_data) {
	char text[128];
	(void)user_data;
	text[0] = '\0';
	if (strcmp(action, "date.changed") == 0) {
		snprintf(text, sizeof(text), "Chosen: %s", Rdate_picker_get_text(sender));
		set_label("date_status", text);
	} else if (strcmp(action, "time.changed") == 0) {
		int h, m, s;
		Rtime_picker_get_time(sender, &h, &m, &s);
		snprintf(text, sizeof(text), "Chosen: %02d:%02d:%02d", h, m, s);
		set_label("time_status", text);
	} else if (strcmp(action, "rating.changed") == 0) {
		snprintf(text, sizeof(text), "%.1f stars", Rrating_get_value(sender));
		set_label("rating_status", text);
	} else if (strcmp(action, "page.changed") == 0) {
		snprintf(text, sizeof(text), "Showing page %d of %d", Rpagination_get_page(sender) + 1, Rpagination_get_page_count(sender));
		set_label("page_status", text);
	} else if (strcmp(action, "color.changed") == 0) {
		snprintf(text, sizeof(text), "Color: %s", Rcolor_picker_get_hex(sender));
		set_label("color_status", text);
	} else if (strcmp(action, "slider.changed") == 0) {
		snprintf(text, sizeof(text), "Volume: %d%%", (int)(Rslider_get_value(sender) * 100.0f + 0.5f));
		set_label("slider_status", text);
	} else if (strcmp(action, "text.commit") == 0) {
		snprintf(text, sizeof(text), "Submitted: \"%s\"", Rtext_input_get_text(sender));
		app_toast(text, R_TOAST_INFO);
	} else if (strcmp(action, "table.striped") == 0) {
		Rtable_set_striped(W("raven_table"), Rcheckbox_is_checked(sender));
	} else if (strcmp(action, "table.grid") == 0) {
		Rtable_set_grid_lines(W("raven_table"), Rcheckbox_is_checked(sender));
	} else if (strcmp(action, "table.header") == 0) {
		Rtable_set_header_visible(W("raven_table"), Rcheckbox_is_checked(sender));
	} else if (strcmp(action, "tree.multi") == 0) {
		Rtree_view_set_multi_select(W("demo_tree"), Rcheckbox_is_checked(sender));
	} else if (strcmp(action, "crumbs.push") == 0) {
		static const char* const folders[] = { "src", "widgets", "deep", "deeper", "deepest", "bottom" };
		Rid crumbs = W("demo_crumbs");
		int count = Rbreadcrumb_get_item_count(crumbs);
		Rbreadcrumb_add_item(crumbs, folders[(count - 1) % 6]);
	} else if (strcmp(action, "crumbs.reset") == 0) {
		Rbreadcrumb_set_items(W("demo_crumbs"), "Macintosh HD|Users|ada|Projects|raven-app");
	}
}

static void on_demo_crumb(Rid breadcrumb, int index, void* user_data) {
	char text[96];
	(void)user_data;
	snprintf(text, sizeof(text), "Went back to \"%s\"", Rbreadcrumb_get_item_text(breadcrumb, index));
	Rbreadcrumb_truncate(breadcrumb, index + 1);
	set_label("crumbs_status", text);
}

/* Every action that has no function of its own. */
static void on_any(Rid sender, const char* action, void* user_data) {
	(void)user_data;
	if (starts_with(action, "say:")) {
		app_toast(action + 4, R_TOAST_INFO);
	} else if (starts_with(action, "go:")) {
		nav_go(action + 3);
	} else if (starts_with(action, "popover:")) {
		popover_toggle(sender, action + 8);
	} else if (starts_with(action, "dt.")) {
		datatable_command(W("datatable"), action + 3);
	} else if (starts_with(action, "file.browse")) {
		browse(sender, action[11] == ':' ? action + 12 : NULL);
	} else if (starts_with(action, "menu.")) {
		char text[96];
		snprintf(text, sizeof(text), "Menu bar: %s", Rwidget_get_type(sender) == R_WIDGET_MENU_ITEM ? Rmenu_item_get_text(sender) : action);
		app_toast(text, R_TOAST_INFO);
	} else if (strcmp(action, "popover.close") == 0) {
		popover_close();
	} else {
		char text[96];
		snprintf(text, sizeof(text), "Action \"%s\"", action);
		app_toast(text, R_TOAST_INFO);
	}
}

void demos_init(void) {
	static const char* const alerts[] = { "alert.info", "alert.confirm", "alert.danger", "alert.save" };
	static const char* const modals[] = { "modal.open", "modal.open.form", "modal.open.terms", "modal.ok", "modal.accept", "modal.cancel" };
	static const char* const toasts[] = { "toast.info", "toast.success", "toast.warning", "toast.error", "toast.action", "toast.burst", "toast.queue", "toast.clear" };
	static const char* const values[] = {
		"date.changed", "time.changed", "rating.changed", "page.changed", "color.changed", "slider.changed", "text.commit",
		"table.striped", "table.grid", "table.header", "tree.multi", "crumbs.push", "crumbs.reset"
	};
	size_t i;
	for (i = 0; i < sizeof(alerts) / sizeof(alerts[0]); i++) Raction_bind(alerts[i], on_alert, NULL);
	for (i = 0; i < sizeof(modals) / sizeof(modals[0]); i++) Raction_bind(modals[i], on_modal, NULL);
	for (i = 0; i < sizeof(toasts) / sizeof(toasts[0]); i++) Raction_bind(toasts[i], on_toast, NULL);
	for (i = 0; i < sizeof(values) / sizeof(values[0]); i++) Raction_bind(values[i], on_value, NULL);
	Raction_bind("progress.start", on_progress, NULL);
	Raction_bind("progress.pause", on_progress, NULL);
	Raction_bind("progress.reset", on_progress, NULL);
	Raction_bind("ctx.item", on_ctx_item, NULL);
	Raction_bind("ctx.open", on_ctx_button, NULL);
	Raction_bind("native.open", on_native_button, NULL);
	Raction_bind("dt.changed", on_dt_changed, NULL);
	Raction_bind("seg.changed", on_segmented, NULL);
	Raction_bind("seg.view", on_segmented, NULL);
	Raction_bind("bg.select", on_group_select, NULL);
	Raction_bind("bg.toggle", on_group_toggle, NULL);
	Raction_set_default_handler(on_any, NULL);
	Rset_frame_callback(on_frame, NULL);

	g_raven_menu = Rwidget_create_from_xml(k_raven_menu_xml, sizeof(k_raven_menu_xml) - 1);
	g_tree_menu = Rwidget_create_from_xml(k_tree_menu_xml, sizeof(k_tree_menu_xml) - 1);
}

/* After the window is built, and again after each live reload. */
void demos_bind_widgets(void) {
	Rid area = W("ctx_area");
	if (area != R_INVALID_ID) Rwidget_set_context_menu(area, g_raven_menu);
	area = W("ctx_area_compare");
	if (area != R_INVALID_ID) Rwidget_set_context_menu(area, g_raven_menu);
	area = W("native_area");
	if (area != R_INVALID_ID) Rwidget_set_event_callback(area, on_native_area_event, NULL);
	area = W("demo_tree");
	if (area != R_INVALID_ID) Rtree_view_set_context_callback(area, on_tree_context, NULL);
	area = W("demo_crumbs");
	if (area != R_INVALID_ID) Rbreadcrumb_set_clicked_callback(area, on_demo_crumb, NULL);
}
