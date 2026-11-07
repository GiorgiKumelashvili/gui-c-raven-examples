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

#include <math.h>
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
	} else if (strcmp(action, "modal.open.edge") == 0 || strcmp(action, "modal.open.hairline") == 0) {
		/* the app's own modal: Raven's dialog would add its frame and margin around the card */
		app_modal_open(strcmp(action, "modal.open.edge") == 0 ? APP_UI_DIR "/dialogs/edge.xml" : APP_UI_DIR "/dialogs/hairline.xml");
	} else if (strcmp(action, "modal.done") == 0) {
		set_label("modal_result", "Closed with its button.");
		if (app_modal_contains(sender)) app_modal_close();
		else Rdialog_close(sender, 1);
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
		if (app_modal_contains(sender)) {
			app_modal_close();
			set_label("modal_result", "The modal was cancelled.");
		} else {
			Rdialog_close(sender, 0);
		}
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

/* ---- widgets with Class="noanim": Raven's animations are global, so they are
 * turned off while such a widget is used and its popup is open ---- */
static int g_noanim;
static int g_noanim_quiet_frames;

static int noanim_hook(const REvent* event) {
	Rid widget;
	if (event->type != R_EVENT_MOUSE_DOWN && event->type != R_EVENT_KEY_DOWN) return R_FALSE;
	for (widget = event->target; widget != R_INVALID_ID; widget = Rwidget_get_parent(widget)) {
		if (Rwidget_has_class(widget, "noanim")) {
			Rset_animations_enabled(R_FALSE);
			g_noanim = 1;
			g_noanim_quiet_frames = 0;
			break;
		}
	}
	return R_FALSE;
}

static void noanim_frame(void) {
	if (!g_noanim) return;
	/* a few frames without any popup nor a button held: the menu, the list or the click is over */
	if (Rwindow_has_popup(g_window) || Rwindow_is_mouse_button_down(g_window, R_MOUSE_LEFT)) g_noanim_quiet_frames = 0;
	else if (++g_noanim_quiet_frames > 3) {
		g_noanim = 0;
		Rset_animations_enabled(R_TRUE);
	}
}

static double g_attention_at;

/*
 * Input adjustments for the whole window, made in the window hook before any
 * widget sees the event:
 *
 *  - Cocoa gives the arrow and function keys characters of the private use
 *    area (0xF700..0xF8FF); Raven passes them on as typed text, so a focused
 *    text field got an invisible character for each Up or Down. Dropped.
 *
 *  - Raven's lists, trees and tables learn the row under the pointer only
 *    when the pointer moves, so while the content scrolls under a still
 *    pointer their hover stays on a row that went away. As in a web page,
 *    the hover of the list being scrolled is hidden during the scroll; when
 *    the scroll stops, the pointer is "moved" where it is (the list finds its
 *    row) and the hover comes back.
 *
 *  - Rails: a trackpad never scrolls perfectly straight, so a vertical swipe
 *    also scrolled a little sideways. Each gesture is locked to the axis it
 *    mostly goes along; scrolling sideways needs a clearly sideways swipe.
 *
 *  - Speeds: a widget with Class="scroll-speed-N" (and what it contains)
 *    scrolls at N percent.
 *
 * Changing the wheel event works because the hook receives the event that
 * Raven then gives to the widgets (window.c: rvn_window_mouse_wheel); the
 * struct is the window's, only the pointer to it is const.
 */
static Rid g_hover_hidden = R_INVALID_ID;
static float g_hover_color[4];
static double g_wheel_last;
static float g_pointer_x, g_pointer_y;

#define RAIL_GAP 0.25      /* seconds without wheel: a new gesture */
#define RAIL_DECIDE 0.35f  /* lines of motion before the axis is chosen */
#define RAIL_RATIO 2.5f    /* sideways must be this many times the vertical motion to win */
static int g_rail;         /* 0 undecided, 1 vertical, 2 horizontal, 3 free (diagonal) */
static float g_rail_x, g_rail_y;

static Rid list_under(Rid widget) {
	for (; widget != R_INVALID_ID; widget = Rwidget_get_parent(widget)) {
		RWidgetType type = Rwidget_get_type(widget);
		if (type == R_WIDGET_LIST_VIEW || type == R_WIDGET_TREE_VIEW || type == R_WIDGET_TABLE || type == R_WIDGET_TILE_VIEW) return widget;
	}
	return R_INVALID_ID;
}

static float speed_of(Rid widget) {
	for (; widget != R_INVALID_ID; widget = Rwidget_get_parent(widget)) {
		const char* classes = Rwidget_get_class(widget);
		const char* found = classes ? strstr(classes, "scroll-speed-") : NULL;
		if (found) return (float)atof(found + 13) / 100.0f;
	}
	return 1.0f;
}

/* inject: R_TRUE outside of an event (a frame), so that the list finds the row under the
 * pointer; inside the hook the real move that is going on does it, and injecting there would nest */
static void hover_restore(int inject) {
	float r, g, b, a;
	if (g_hover_hidden == R_INVALID_ID) return;
	if (inject) Rwindow_inject_mouse_move(g_window, g_pointer_x, g_pointer_y);
	Rwidget_clear_color(g_hover_hidden, R_COLOR_ROW_HOVERED);
	Rwidget_get_color(g_hover_hidden, R_COLOR_ROW_HOVERED, &r, &g, &b, &a);
	/* a list that had a color of its own in its XML (the sidebar) gets it back */
	if (r != g_hover_color[0] || g != g_hover_color[1] || b != g_hover_color[2] || a != g_hover_color[3]) {
		Rwidget_set_color(g_hover_hidden, R_COLOR_ROW_HOVERED, g_hover_color[0], g_hover_color[1], g_hover_color[2], g_hover_color[3]);
	}
	g_hover_hidden = R_INVALID_ID;
}

static void rails(REvent* event, double now) {
	float ax, ay;
	if (now - g_wheel_last > RAIL_GAP) {
		g_rail = 0;
		g_rail_x = g_rail_y = 0.0f;
	}
	ax = fabsf(event->wheel_x);
	ay = fabsf(event->wheel_y);
	if (g_rail == 0) {
		g_rail_x += ax;
		g_rail_y += ay;
		if (g_rail_x + g_rail_y >= RAIL_DECIDE) {
			if (g_rail_x > g_rail_y * RAIL_RATIO) g_rail = 2;
			else if (g_rail_y > g_rail_x * 0.4f) g_rail = 1;
			else g_rail = 3;
		}
	}
	/* before the decision, and on the vertical rail, the sideways part goes */
	if (g_rail == 0 || g_rail == 1) event->wheel_x = 0.0f;
	else if (g_rail == 2) event->wheel_y = 0.0f;
}

static int input_fix_hook(const REvent* event) {
	if (event->type == R_EVENT_TEXT && event->codepoint >= 0xF700 && event->codepoint <= 0xF8FF) return R_TRUE;
	if (event->type == R_EVENT_MOUSE_WHEEL) {
		REvent* wheel = (REvent*)event;
		double now = Rget_time();
		Rid list;
		float speed;
		/* Shift + wheel is sideways on purpose: no rails then */
		if (!(event->modifiers & R_MOD_SHIFT)) rails(wheel, now);
		speed = speed_of(event->target);
		wheel->wheel_x *= speed;
		wheel->wheel_y *= speed;
		g_wheel_last = now;
		g_pointer_x = event->x;
		g_pointer_y = event->y;
		list = list_under(event->target);
		if (list != g_hover_hidden) {
			hover_restore(R_FALSE);
			if (list != R_INVALID_ID) {
				Rwidget_get_color(list, R_COLOR_ROW_HOVERED, &g_hover_color[0], &g_hover_color[1], &g_hover_color[2], &g_hover_color[3]);
				Rwidget_set_color(list, R_COLOR_ROW_HOVERED, 0.0f, 0.0f, 0.0f, 0.0f);
				g_hover_hidden = list;
			}
		}
		Rwake_up();
	} else if (event->type == R_EVENT_MOUSE_MOVE) {
		g_pointer_x = event->x;
		g_pointer_y = event->y;
		/* the pointer moved by itself (not the move this file injects): the hover may come back */
		if (g_hover_hidden != R_INVALID_ID && (event->delta_x != 0.0f || event->delta_y != 0.0f) && Rget_time() - g_wheel_last > 0.08) {
			hover_restore(R_FALSE);
		}
	}
	return R_FALSE;
}

/* The terminal laid over the dock area: its top edge is dragged (layout.dock.xml). */
static int g_dock_resizing;
static float g_dock_start_y, g_dock_start_h;

static int dock_resize_hook(const REvent* event) {
	Rid console, grip, area;
	float x, y, w, h, ay, ah, height;
	if (event->type == R_EVENT_MOUSE_DOWN && event->button == R_MOUSE_LEFT) {
		grip = W("dock4_grip");
		if (grip == R_INVALID_ID || Rwidget_get_visibility(W("dock4_console")) != R_VISIBLE) return R_FALSE;
		Rwidget_get_rect(grip, &x, &y, &w, &h);
		if (event->x < x || event->x >= x + w || event->y < y - 2.0f || event->y >= y + h + 2.0f) return R_FALSE;
		Rwidget_get_rect(W("dock4_console"), &x, &y, &w, &h);
		g_dock_resizing = 1;
		g_dock_start_y = event->y;
		g_dock_start_h = h;
		return R_TRUE;
	}
	if (!g_dock_resizing) return R_FALSE;
	if (event->type == R_EVENT_MOUSE_MOVE) {
		console = W("dock4_console");
		area = W("dock4_area");
		Rwidget_get_rect(area, &x, &ay, &w, &ah);
		height = g_dock_start_h - (event->y - g_dock_start_y);
		if (height < 70.0f) height = 70.0f;
		if (height > ah - 30.0f) height = ah - 30.0f;
		Rwidget_set_size(console, -1.0f, height);
		return R_TRUE;
	}
	if (event->type == R_EVENT_MOUSE_UP) {
		g_dock_resizing = 0;
		return R_TRUE;
	}
	return R_FALSE;
}

/* Each frame: a scroll that ended gives the hover back. */
static void input_fix_frame(void) {
	if (g_hover_hidden == R_INVALID_ID) return;
	if (Rget_time() - g_wheel_last > 0.3) hover_restore(R_TRUE);
	else Rwake_up();
}

static void on_frame(double delta, void* user_data) {
	char text[48];
	(void)user_data;
	noanim_frame();
	reorder_frame();
	input_fix_frame();
	if (g_attention_at > 0.0 && Rget_time() >= g_attention_at) {
		g_attention_at = 0.0;
		native_request_attention();
	}
	if (g_noanim || g_attention_at > 0.0) Rwake_up();
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
 *  macOS: Finder panels and notifications
 * ------------------------------------------------------------------------- */

static Rid g_finder_label = R_INVALID_ID;
static char g_finder_path[1024];

static void on_finder_paths(int count, const char* const* paths, void* user_data) {
	char text[1200];
	int i, length = 0;
	(void)user_data;
	if (count == 0) {
		set_label("finder_status", "Cancelled.");
		return;
	}
	snprintf(g_finder_path, sizeof(g_finder_path), "%s", paths[0]);
	if (g_finder_label != R_INVALID_ID) {
		const char* slash = strrchr(paths[0], '/');
		if (count > 1) snprintf(text, sizeof(text), "%d files chosen", count);
		else snprintf(text, sizeof(text), "%s", slash && slash[1] ? slash + 1 : paths[0]);
		Rlabel_set_text(g_finder_label, text);
		Rwidget_set_attribute(g_finder_label, "Opacity", "1");
	}
	text[0] = '\0';
	for (i = 0; i < count && i < 6 && length < (int)sizeof(text) - 1; i++) {
		length += snprintf(text + length, sizeof(text) - (size_t)length, "%s%s", i ? "\n" : "", paths[i]);
	}
	if (count > 6 && length < (int)sizeof(text) - 1) snprintf(text + length, sizeof(text) - (size_t)length, "\n... and %d more", count - 6);
	set_label("finder_status", text);
	Rwidget_set_enabled(W("finder_reveal"), R_TRUE);
}

static void on_finder(Rid sender, const char* action, void* user_data) {
	(void)user_data;
	if (strncmp(action, "finder.open", 11) == 0) {
		/* the label of the field is the first child of the row of the button */
		Rid row = Rwidget_get_parent(sender);
		g_finder_label = row != R_INVALID_ID ? Rwidget_get_child(row, 0) : R_INVALID_ID;
		if (!native_open_panel(g_window, action[11] == ':' ? action + 12 : NULL, on_finder_paths, NULL)) {
			set_label("finder_status", "No Finder panel on this system.");
		}
	} else if (strcmp(action, "finder.save") == 0) {
		g_finder_label = R_INVALID_ID;
		native_save_panel(g_window, "Untitled report.pdf", on_finder_paths, NULL);
	} else if (strcmp(action, "finder.reveal") == 0) {
		native_reveal_in_finder(g_finder_path);
	} else if (strcmp(action, "finder.home") == 0) {
		native_open_in_finder("~");
	} else if (strcmp(action, "finder.downloads") == 0) {
		native_open_in_finder("~/Downloads");
	}
}

static void on_notify_status(const char* status, void* user_data) {
	(void)user_data;
	set_label("notify_status", status);
}

static void on_notify(Rid sender, const char* action, void* user_data) {
	static int badge;
	char text[16];
	(void)sender;
	(void)user_data;
	if (strcmp(action, "notify.simple") == 0) {
		native_notify("Raven Components", "", "Hello from a Raven app: this is the Notification Center of macOS.", 0, 0.0, on_notify_status, NULL);
	} else if (strcmp(action, "notify.rich") == 0) {
		native_notify("Build finished", "raven_app - Release", "128 files compiled in 8.2 s. 0 warnings.", 1, 0.0, on_notify_status, NULL);
	} else if (strcmp(action, "notify.delayed") == 0) {
		native_notify("Reminder", "Stand-up in 5 minutes", "Scheduled 5 seconds ago. Switch to another app to see it arrive.", 1, 5.0, on_notify_status, NULL);
		set_label("notify_status", "Scheduling...");
	} else if (strcmp(action, "notify.badge") == 0) {
		snprintf(text, sizeof(text), "%d", ++badge);
		native_dock_badge(text);
		set_label("notify_status", "Look at the Dock icon.");
	} else if (strcmp(action, "notify.badge.clear") == 0) {
		badge = 0;
		native_dock_badge(NULL);
		set_label("notify_status", "Badge cleared.");
	} else if (strcmp(action, "notify.settings") == 0) {
		native_open_notification_settings();
	} else if (strcmp(action, "notify.attention") == 0) {
		g_attention_at = Rget_time() + 3.0;
		set_label("notify_status", "Switch to another app now: the Dock icon bounces in 3 seconds.");
	}
}

static void on_history(Rid sender, const char* action, void* user_data) {
	(void)sender;
	(void)user_data;
	if (strcmp(action, "nav.back") == 0) nav_back();
	else nav_forward();
}

/* Trackpad swipes and the side buttons of a mouse go back and forward. */
static int history_hook(const REvent* event) {
	if (event->type != R_EVENT_MOUSE_DOWN) return R_FALSE;
	if (event->button == R_MOUSE_X1) nav_back();
	else if (event->button == R_MOUSE_X2) nav_forward();
	else return R_FALSE;
	return R_TRUE;
}

static void on_textarea(Rid sender, const char* action, void* user_data) {
	char text[48];
	int length = Rtext_editor_get_length(sender);
	(void)action;
	(void)user_data;
	snprintf(text, sizeof(text), "%d / 280", length);
	set_label("textarea_count", text);
	Rwidget_set_attribute(W("textarea_count"), "Color", length > 280 ? "#E5484D" : "#8B8D98");
}

/* ---------------------------------------------------------------------------
 *  Small things of the other pages
 * ------------------------------------------------------------------------- */

static void on_value(Rid sender, const char* action, void* user_data) {
	char text[128];
	(void)user_data;
	text[0] = '\0';
	if (strcmp(action, "date.changed") == 0) {
		const char* name = Rwidget_get_name(sender);
		snprintf(text, sizeof(text), "Chosen: %s", Rdate_picker_get_text(sender));
		set_label("date_status", text);
		if (name && strcmp(name, "jump_picker") == 0) {
			/* the calendar moved: the month and the year follow */
			int y, m, d;
			Rdate_picker_get_date(sender, &y, &m, &d);
			Rcombo_box_set_selected(W("jump_month"), m - 1);
			Rspin_box_set_value(W("jump_year"), y);
		}
	} else if (strcmp(action, "date.jump") == 0) {
		/* a month or a year chosen: the date goes there, the day kept when the month has it */
		int y, m, d, days;
		Rid picker = W("jump_picker");
		Rdate_picker_get_date(picker, &y, &m, &d);
		m = Rcombo_box_get_selected(W("jump_month")) + 1;
		y = (int)Rspin_box_get_value(W("jump_year"));
		days = (m == 2) ? ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 29 : 28) : (m == 4 || m == 6 || m == 9 || m == 11) ? 30 : 31;
		if (d > days) d = days;
		Rdate_picker_set_date(picker, y, m, d);
		snprintf(text, sizeof(text), "Chosen: %s", Rdate_picker_get_text(picker));
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

/* Neobrutalism accordion: one item open at a time, its header yellow. */
static void neo_open(int index) {
	char name[32];
	int i, open;
	for (i = 0; i < 16; i++) {
		Rid body, head, sign;
		snprintf(name, sizeof(name), "neo_body_%d", i);
		body = W(name);
		if (body == R_INVALID_ID) continue;
		snprintf(name, sizeof(name), "neo_head_%d", i);
		head = W(name);
		snprintf(name, sizeof(name), "neo_sign_%d", i);
		sign = W(name);
		/* the one clicked toggles; the others close */
		open = i == index && Rwidget_get_visibility(body) != R_VISIBLE;
		Rwidget_set_visibility(body, open ? R_VISIBLE : R_COLLAPSED);
		if (head != R_INVALID_ID) Rborder_set_background(head, open ? 1.0f : 1.0f, open ? 0.87f : 1.0f, open ? 0.10f : 1.0f, 1.0f);
		if (sign != R_INVALID_ID) Rlabel_set_text(sign, open ? "-" : "+");
	}
}

/* Raven's Expanders made an accordion: the one that opens closes its siblings. */
static void accordion_exclusive(Rid expander) {
	Rid parent = Rwidget_get_parent(expander);
	int i;
	if (!Rexpander_is_expanded(expander) || parent == R_INVALID_ID) return;
	for (i = 0; i < Rwidget_get_child_count(parent); i++) {
		Rid sibling = Rwidget_get_child(parent, i);
		if (sibling != expander && Rwidget_get_type(sibling) == R_WIDGET_EXPANDER) Rexpander_set_expanded(sibling, R_FALSE);
	}
}

/* A month picker made of buttons: a year with arrows over the twelve months. */
static int g_mg_year = 2026, g_mg_month = 11;

static void month_grid(const char* command) {
	static const char* const months[12] = { "January", "February", "March", "April", "May", "June",
		"July", "August", "September", "October", "November", "December" };
	Rid grid = W("mg_months");
	char text[64];
	int i;
	if (strcmp(command, "prev") == 0) g_mg_year--;
	else if (strcmp(command, "next") == 0) g_mg_year++;
	else if (starts_with(command, "month:")) g_mg_month = atoi(command + 6);
	snprintf(text, sizeof(text), "%d", g_mg_year);
	set_label("mg_year", text);
	for (i = 0; grid != R_INVALID_ID && i < Rwidget_get_child_count(grid); i++) {
		Rbutton_set_style(Rwidget_get_child(grid, i), i + 1 == g_mg_month ? R_BUTTON_PRIMARY : R_BUTTON_FLAT);
	}
	snprintf(text, sizeof(text), "Chosen: %s %d", months[g_mg_month - 1], g_mg_year);
	set_label("mg_status", text);
	if (starts_with(command, "month:")) {
		Rid picker = W("jump_picker");
		if (picker != R_INVALID_ID) Rdate_picker_set_date(picker, g_mg_year, g_mg_month, 1);
	}
}

/* Every action that has no function of its own. */
static void on_any(Rid sender, const char* action, void* user_data) {
	(void)user_data;
	if (starts_with(action, "say:")) {
		app_toast(action + 4, R_TOAST_INFO);
	} else if (starts_with(action, "neo:")) {
		neo_open(atoi(action + 4));
	} else if (strcmp(action, "accordion.exclusive") == 0) {
		accordion_exclusive(sender);
	} else if (starts_with(action, "mg.")) {
		month_grid(action + 3);
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
	static const char* const modals[] = { "modal.open", "modal.open.form", "modal.open.terms", "modal.ok", "modal.accept", "modal.cancel",
		"modal.open.edge", "modal.open.hairline", "modal.done" };
	static const char* const toasts[] = { "toast.info", "toast.success", "toast.warning", "toast.error", "toast.action", "toast.burst", "toast.queue", "toast.clear" };
	static const char* const values[] = {
		"date.changed", "date.jump", "time.changed", "rating.changed", "page.changed", "color.changed", "slider.changed", "text.commit",
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
	{
		static const char* const finder[] = { "finder.open", "finder.open:images", "finder.open:documents", "finder.open:multi",
			"finder.open:folder", "finder.save", "finder.reveal", "finder.home", "finder.downloads" };
		static const char* const notify[] = { "notify.simple", "notify.rich", "notify.delayed",
			"notify.badge", "notify.badge.clear", "notify.attention", "notify.settings" };
		for (i = 0; i < sizeof(finder) / sizeof(finder[0]); i++) Raction_bind(finder[i], on_finder, NULL);
		for (i = 0; i < sizeof(notify) / sizeof(notify[0]); i++) Raction_bind(notify[i], on_notify, NULL);
	}
	Raction_bind("nav.back", on_history, NULL);
	Raction_bind("nav.forward", on_history, NULL);
	Raction_bind("textarea.changed", on_textarea, NULL);
	app_add_event_hook(input_fix_hook);
	app_add_event_hook(dock_resize_hook);
	app_add_event_hook(noanim_hook);
	app_add_event_hook(history_hook);
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
