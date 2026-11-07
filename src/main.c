/*
 * Raven components: a gallery of what Raven can do.
 *
 *   main.xml          the window: native menu bar, sidebar (tree), content
 *   widgets.xml       tags declared in XML (Page, Demo, ButtonGroup, FileInput...)
 *   pages/ *.xml      one page per component, live reloaded when saved
 *   nav.c             sidebar tree, filter, breadcrumb, page switching
 *   demos.c           the actions of the pages
 *   datatable.c       <DataTable>         custom painted
 *   segmented.c       <SegmentedControl>  custom painted
 *   popover.c         popover anchored to a widget
 *   native_menu_*.m   the context menu of macOS (NSMenu)
 */

#include <stdio.h>
#include <stdlib.h>
#include "app.h"

Rid g_window = R_INVALID_ID;

#define MAX_HOOKS 32
static AppEventHook g_hooks[MAX_HOOKS];
static int g_hook_count;

void app_add_event_hook(AppEventHook hook) {
	if (g_hook_count == MAX_HOOKS) {
		/* a hook that silently does nothing is a bug hard to find: say it */
		fprintf(stderr, "[app] too many event hooks (%d): raise MAX_HOOKS in main.c\n", MAX_HOOKS);
		return;
	}
	g_hooks[g_hook_count++] = hook;
}

static int window_hook(Rid window, const REvent* event, void* user_data) {
	int i;
	(void)window;
	(void)user_data;
	for (i = 0; i < g_hook_count; i++) {
		if (g_hooks[i](event)) return R_TRUE;
	}
	return R_FALSE;
}

void app_toast(const char* text, RToastKind kind) {
	Rid toast = Rwidget_find(g_window, "toast");
	if (toast != R_INVALID_ID) Rtoast_show_kind(toast, text, kind, 3.0);
}

/* Debug: APP_SHOT=/tmp/x.tga writes the window into a TGA after a moment and quits. */
static void save_tga(const char* path) {
	int width = 0, height = 0, x, y;
	uint8_t* pixels = Rwindow_capture(g_window, &width, &height);
	FILE* file;
	unsigned char header[18] = { 0 };
	if (!pixels) return;
	file = fopen(path, "wb");
	if (file) {
		header[2] = 2;
		header[12] = (unsigned char)(width & 255);
		header[13] = (unsigned char)(width >> 8);
		header[14] = (unsigned char)(height & 255);
		header[15] = (unsigned char)(height >> 8);
		header[16] = 32;
		header[17] = 0x28; /* top-left origin, 8 alpha bits */
		fwrite(header, 1, sizeof(header), file);
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				const uint8_t* p = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4;
				unsigned char bgra[4] = { p[2], p[1], p[0], p[3] };
				fwrite(bgra, 1, 4, file);
			}
		}
		fclose(file);
	}
	Rpixels_free(pixels);
}

static void on_error(RError code, RSeverity severity, const char* detail) {
	fprintf(stderr, "[Raven] %s (%s): %s\n", Rerror_to_string(code), Rseverity_to_string(severity), detail);
}

/* A save of main.xml or of a page may rebuild widgets: look them up again. */
static void on_reload(const char* path, int ok, void* user_data) {
	(void)path;
	(void)user_data;
	if (!ok) return;
	nav_init();
	demos_bind_widgets();
	omnibar_bind_widgets();
	vlist_bind_widgets();
	http_bind_widgets();
	tooltip_scan();
	scroll_contain_bind();
}

int main(void) {
	RInitDesc desc = { 0 };
	desc.backend = R_BACKEND_VULKAN;
	desc.app_name = "Raven Components";
	Rerror_set_callback(on_error);
	if (!Rinit_ex(&desc)) return 1;

	/* documents given as text (header rows of the HTTP page) find their files in src/ */
	Rxml_set_base_path(APP_UI_DIR);

	/* the tags made in C must be known before the documents that use them */
	datatable_register();
	segmented_register();
	canvas_demo_register();
	roundlist_register();
	vlist_init();
	http_init();
	demos_init();
	popover_init();
	reorder_init();
	omnibar_init();
	app_modal_init();
	tooltip_init();
	scroll_contain_init();

	/* APP_THEME=MacOSLight: start with another look */
	if (getenv("APP_THEME")) Rtheme_use(getenv("APP_THEME"));

	g_window = Rwindow_create_from_xml_file(APP_UI_DIR "/main.xml");
	if (g_window == R_INVALID_ID) {
		Rshutdown();
		return 1;
	}
	Rwindow_set_event_hook(g_window, window_hook, NULL);
	Rxml_set_reload_callback(on_reload, NULL);

	nav_init();
	demos_bind_widgets();
	omnibar_bind_widgets();
	vlist_bind_widgets();
	http_bind_widgets();
	tooltip_scan();
	scroll_contain_bind();
	/* APP_PAGE=custom.datatable ./build/raven_app opens on that page */
	if (getenv("APP_PAGE")) nav_go(getenv("APP_PAGE"));

	if (getenv("APP_SHOT")) {
		double start = Rget_time();
		if (getenv("APP_TEST")) selftest_run(getenv("APP_TEST"));
		while (Rwait_frame(0.05) && Rget_time() - start < 2.5) {
		}
		save_tga(getenv("APP_SHOT"));
	} else {
		while (Rwait_frame(-1.0)) {
		}
	}

	Rshutdown();
	return 0;
}
