/* Other systems: no native context menu here; the caller falls back to a Raven menu. */
#include "app.h"

int native_context_menu(Rid window, float x, float y, const char* const* items, int count) {
	(void)window;
	(void)x;
	(void)y;
	(void)items;
	(void)count;
	return -2;
}
