/*
 * The context menu of macOS (NSMenu), opened over a Raven window.
 *
 * Raven draws its own menus; this one is the menu of the system: its blur,
 * its fonts, its keyboard navigation, its SF Symbols. Raven gives the
 * NSWindow (Rwindow_get_native_handle); the menu is popped up in its
 * content view and tracked by AppKit until the user picks or dismisses.
 *
 * An item is "Title", "symbol.name|Title" (an SF Symbol before the title)
 * or "-" (a separator).
 */

#import <AppKit/AppKit.h>
#include <string.h>
#include "app.h"

@interface AppNativeMenuTarget : NSObject
@property(nonatomic) NSInteger chosen;
- (void)pick:(NSMenuItem*)sender;
@end

@implementation AppNativeMenuTarget
- (void)pick:(NSMenuItem*)sender {
	self.chosen = sender.tag;
}
@end

int native_context_menu(Rid window, float x, float y, const char* const* items, int count) {
	RNativeWindow native;
	NSWindow* ns_window;
	NSView* view;
	NSMenu* menu;
	AppNativeMenuTarget* target;
	NSPoint point;
	int i;

	memset(&native, 0, sizeof(native));
	if (!Rwindow_get_native_handle(window, &native) || native.system != R_WINDOW_SYSTEM_COCOA || !native.display) return -2;
	ns_window = (__bridge NSWindow*)native.display;
	view = ns_window.contentView;
	if (!view) return -2;

	target = [AppNativeMenuTarget new];
	target.chosen = -1;
	menu = [[NSMenu alloc] initWithTitle:@""];
	menu.autoenablesItems = NO;

	for (i = 0; i < count; i++) {
		const char* spec = items[i];
		const char* bar = strchr(spec, '|');
		NSString* title;
		NSMenuItem* item;

		if (strcmp(spec, "-") == 0) {
			[menu addItem:[NSMenuItem separatorItem]];
			continue;
		}
		title = [NSString stringWithUTF8String:bar ? bar + 1 : spec];
		item = [[NSMenuItem alloc] initWithTitle:title action:@selector(pick:) keyEquivalent:@""];
		item.target = target;
		item.tag = i;
		if (bar) {
			NSString* symbol = [[NSString alloc] initWithBytes:spec length:(NSUInteger)(bar - spec) encoding:NSUTF8StringEncoding];
			if (@available(macOS 11.0, *)) {
				item.image = [NSImage imageWithSystemSymbolName:symbol accessibilityDescription:title];
			}
		}
		[menu addItem:item];
	}

	/* Raven: logical units from the top-left corner of the client area (points on macOS) */
	point = NSMakePoint(x, view.isFlipped ? y : view.bounds.size.height - y);
	[menu popUpMenuPositioningItem:nil atLocation:point inView:view];
	return (int)target.chosen;
}
