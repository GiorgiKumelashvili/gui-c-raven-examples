/*
 * macOS services that Raven does not wrap, called from C (app.h):
 *
 *   - the Finder panels: NSOpenPanel / NSSavePanel, as sheets of the window
 *     (Raven's Rfile_dialog_open shows its own browser on macOS),
 *   - Finder itself: reveal a file, open a folder,
 *   - notifications of the Notification Center (UNUserNotificationCenter),
 *     with an AppleScript fallback when the app is not an allowed bundle,
 *   - the badge of the Dock icon.
 *
 * Everything runs on the main thread, which is Raven's UI thread on macOS;
 * what the system answers on other threads comes back with Rtask_post.
 */

#import <AppKit/AppKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <UserNotifications/UserNotifications.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"

static NSWindow* ns_window_of(Rid window) {
	RNativeWindow native;
	memset(&native, 0, sizeof(native));
	if (!Rwindow_get_native_handle(window, &native) || native.system != R_WINDOW_SYSTEM_COCOA) return nil;
	return (__bridge NSWindow*)native.display;
}

static NSString* ns_string(const char* text) {
	return [NSString stringWithUTF8String:text ? text : ""];
}

/* ---------------------------------------------------------------------------
 *  Finder panels
 * ------------------------------------------------------------------------- */

static void deliver_paths(NSArray<NSURL*>* urls, NativePathsCallback callback, void* user_data) {
	int count = (int)urls.count, i;
	const char** paths = (const char**)calloc((size_t)(count > 0 ? count : 1), sizeof(char*));
	for (i = 0; i < count; i++) paths[i] = urls[(NSUInteger)i].path.UTF8String;
	callback(count, paths, user_data);
	free(paths);
	Rwake_up();
}

int native_open_panel(Rid window, const char* kind, NativePathsCallback callback, void* user_data) {
	NSWindow* ns_window = ns_window_of(window);
	NSOpenPanel* panel;
	if (!ns_window) return R_FALSE;

	panel = [NSOpenPanel openPanel];
	panel.canChooseFiles = YES;
	panel.canChooseDirectories = NO;
	panel.allowsMultipleSelection = NO;
	panel.prompt = @"Choose";
	if (kind && strcmp(kind, "folder") == 0) {
		panel.canChooseFiles = NO;
		panel.canChooseDirectories = YES;
		panel.canCreateDirectories = YES;
		panel.message = @"Choose a folder";
	} else if (kind && strcmp(kind, "multi") == 0) {
		panel.allowsMultipleSelection = YES;
		panel.message = @"Choose one or more files (Cmd-click for several)";
	} else if (kind && strcmp(kind, "images") == 0) {
		if (@available(macOS 11.0, *)) panel.allowedContentTypes = @[ UTTypeImage ];
		panel.message = @"Choose an image";
	} else if (kind && strcmp(kind, "documents") == 0) {
		if (@available(macOS 11.0, *)) {
			NSMutableArray<UTType*>* types = [NSMutableArray arrayWithObjects:UTTypePDF, UTTypePlainText, UTTypeXML, UTTypeJSON, nil];
			UTType* markdown = [UTType typeWithFilenameExtension:@"md"];
			if (markdown) [types addObject:markdown];
			panel.allowedContentTypes = types;
		}
		panel.message = @"Choose a document: PDF, text, Markdown, XML or JSON";
	}

	/* a sheet: slides out of the title bar, the window stays usable around it */
	[panel beginSheetModalForWindow:ns_window completionHandler:^(NSModalResponse response) {
		deliver_paths(response == NSModalResponseOK ? panel.URLs : @[], callback, user_data);
	}];
	return R_TRUE;
}

int native_save_panel(Rid window, const char* file_name, NativePathsCallback callback, void* user_data) {
	NSWindow* ns_window = ns_window_of(window);
	NSSavePanel* panel;
	if (!ns_window) return R_FALSE;
	panel = [NSSavePanel savePanel];
	panel.nameFieldStringValue = ns_string(file_name);
	panel.canCreateDirectories = YES;
	panel.showsTagField = YES;
	[panel beginSheetModalForWindow:ns_window completionHandler:^(NSModalResponse response) {
		deliver_paths(response == NSModalResponseOK && panel.URL ? @[ panel.URL ] : @[], callback, user_data);
	}];
	return R_TRUE;
}

int native_reveal_in_finder(const char* path) {
	NSURL* url;
	if (!path || !path[0]) return R_FALSE;
	url = [NSURL fileURLWithPath:ns_string(path)];
	[[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ url ]];
	return R_TRUE;
}

int native_open_in_finder(const char* folder) {
	NSString* path = [ns_string(folder) stringByExpandingTildeInPath];
	return [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:path isDirectory:YES]] ? R_TRUE : R_FALSE;
}

/* ---------------------------------------------------------------------------
 *  Notifications
 * ------------------------------------------------------------------------- */

/* Shows the notifications while the app is in front too (by default the center keeps them quiet). */
@interface AppNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
@end

@implementation AppNotificationDelegate
- (void)userNotificationCenter:(UNUserNotificationCenter*)center
	   willPresentNotification:(UNNotification*)notification
		 withCompletionHandler:(void (^)(UNNotificationPresentationOptions))handler API_AVAILABLE(macos(10.14)) {
	(void)center;
	(void)notification;
	handler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList | UNNotificationPresentationOptionSound);
}
@end

typedef struct NotifyReport {
	NativeNotifyCallback callback;
	void* user_data;
	char* status;
} NotifyReport;

static void notify_report_on_ui(void* data) {
	NotifyReport* report = (NotifyReport*)data;
	if (report->callback) report->callback(report->status, report->user_data);
	free(report->status);
	free(report);
}

/* From any thread: the status reaches the C callback on the UI thread. */
static void notify_report(NativeNotifyCallback callback, void* user_data, NSString* status) {
	NotifyReport* report = (NotifyReport*)calloc(1, sizeof(NotifyReport));
	report->callback = callback;
	report->user_data = user_data;
	report->status = strdup(status.UTF8String);
	Rtask_post(notify_report_on_ui, report);
}

static NSString* applescript_quote(NSString* text) {
	return [[text stringByReplacingOccurrencesOfString:@"\\" withString:@"\\\\"] stringByReplacingOccurrencesOfString:@"\"" withString:@"\\\""];
}

/* Works for any process, but the notification is shown as coming from Script Editor, with the
 * settings of Script Editor (its sound may be off): the sound is played here instead. */
static BOOL notify_applescript(NSString* title, NSString* subtitle, NSString* body, BOOL sound) {
	NSString* source = subtitle.length
		? [NSString stringWithFormat:@"display notification \"%@\" with title \"%@\" subtitle \"%@\"",
			applescript_quote(body), applescript_quote(title), applescript_quote(subtitle)]
		: [NSString stringWithFormat:@"display notification \"%@\" with title \"%@\"", applescript_quote(body), applescript_quote(title)];
	NSDictionary* error = nil;
	[[[NSAppleScript alloc] initWithSource:source] executeAndReturnError:&error];
	if (sound) [[NSSound soundNamed:@"Glass"] play];
	return error == nil;
}

int native_open_notification_settings(void) {
	NSString* id_ = NSBundle.mainBundle.bundleIdentifier ?: @"";
	NSURL* url = [NSURL URLWithString:[@"x-apple.systempreferences:com.apple.Notifications-Settings.extension?id=" stringByAppendingString:id_]];
	return [[NSWorkspace sharedWorkspace] openURL:url] ? R_TRUE : R_FALSE;
}

int native_notify(const char* title, const char* subtitle, const char* body, int sound, double delay,
	NativeNotifyCallback callback, void* user_data) {
	static AppNotificationDelegate* delegate;
	NSString* ns_title = ns_string(title);
	NSString* ns_subtitle = ns_string(subtitle);
	NSString* ns_body = ns_string(body);
	UNUserNotificationCenter* center;

	/* the Notification Center only serves application bundles (see CMakeLists.txt: raven_app.app) */
	if (!NSBundle.mainBundle.bundleIdentifier) {
		BOOL ok = notify_applescript(ns_title, ns_subtitle, ns_body, sound != 0);
		notify_report(callback, user_data, ok ? @"Shown with AppleScript (the app is not a bundle)" : @"AppleScript failed");
		return ok ? R_TRUE : R_FALSE;
	}

	center = [UNUserNotificationCenter currentNotificationCenter];
	if (!delegate) {
		delegate = [AppNotificationDelegate new];
		center.delegate = delegate;
	}
	/* the first time, macOS asks the user; afterwards it answers at once */
	[center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert | UNAuthorizationOptionSound | UNAuthorizationOptionBadge)
						  completionHandler:^(BOOL granted, NSError* error) {
		UNMutableNotificationContent* content;
		UNNotificationRequest* request;
		UNTimeIntervalNotificationTrigger* trigger = nil;
		if (!granted) {
			NSString* reason = @"they are turned off for Raven Components: System Settings, Notifications (button below)";
			(void)error;
			dispatch_async(dispatch_get_main_queue(), ^{
				BOOL ok = notify_applescript(ns_title, ns_subtitle, ns_body, sound != 0);
				notify_report(callback, user_data, [NSString stringWithFormat:@"Notification Center refused: %@.%@", reason,
					ok ? @" Shown through AppleScript meanwhile (as Script Editor, with its own settings)." : @""]);
			});
			Rwake_up();
			return;
		}
		content = [UNMutableNotificationContent new];
		content.title = ns_title;
		if (ns_subtitle.length) content.subtitle = ns_subtitle;
		content.body = ns_body;
		if (sound) content.sound = [UNNotificationSound defaultSound];
		if (delay > 0.0) trigger = [UNTimeIntervalNotificationTrigger triggerWithTimeInterval:delay repeats:NO];
		request = [UNNotificationRequest requestWithIdentifier:[NSUUID UUID].UUIDString content:content trigger:trigger];
		[center addNotificationRequest:request withCompletionHandler:^(NSError* add_error) {
			notify_report(callback, user_data, add_error ? add_error.localizedDescription
				: (delay > 0.0 ? [NSString stringWithFormat:@"Scheduled in %.0f s", delay] : @"Delivered to the Notification Center"));
		}];
	}];
	return R_TRUE;
}

int native_dock_badge(const char* text) {
	NSApp.dockTile.badgeLabel = text && text[0] ? ns_string(text) : nil;
	return R_TRUE;
}

int native_request_attention(void) {
	/* bounces the Dock icon, only while the app is not the active one */
	[NSApp requestUserAttention:NSInformationalRequest];
	return R_TRUE;
}

/* ---------------------------------------------------------------------------
 *  HTTP (NSURLSession), JSON answers only
 * ------------------------------------------------------------------------- */

typedef struct HttpResult {
	NativeHttpCallback callback;
	void* user_data;
	int status;
	double ms;
	char* headers;
	char* body;
	char* error;
} HttpResult;

static void http_on_ui(void* data) {
	HttpResult* r = (HttpResult*)data;
	r->callback(r->status, r->ms, r->headers, r->body, r->error, r->user_data);
	free(r->headers);
	free(r->body);
	free(r->error);
	free(r);
}

static char* dup_ns(NSString* text) {
	return text ? strdup(text.UTF8String ?: "") : NULL;
}

int native_http(const char* method, const char* url, const char* const* names, const char* const* values, int count,
	const char* body, NativeHttpCallback callback, void* user_data) {
	NSURL* ns_url = [NSURL URLWithString:[ns_string(url) stringByTrimmingCharactersInSet:NSCharacterSet.whitespaceCharacterSet]];
	NSMutableURLRequest* request;
	NSString* verb = ns_string(method).uppercaseString;
	BOOL has_accept = NO;
	CFAbsoluteTime start;
	int i;

	if (!ns_url || !ns_url.scheme || !ns_url.host) {
		HttpResult* r = (HttpResult*)calloc(1, sizeof(HttpResult));
		r->callback = callback;
		r->user_data = user_data;
		r->error = strdup("Not a valid URL: it needs a scheme and a host (https://example.com/...)");
		Rtask_post(http_on_ui, r);
		return R_TRUE;
	}
	request = [NSMutableURLRequest requestWithURL:ns_url cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:20.0];
	request.HTTPMethod = verb;
	for (i = 0; i < count; i++) {
		NSString* name = ns_string(names[i]);
		if (!name.length) continue;
		if ([name caseInsensitiveCompare:@"Accept"] == NSOrderedSame) has_accept = YES;
		[request setValue:ns_string(values[i]) forHTTPHeaderField:name];
	}
	if (!has_accept) [request setValue:@"application/json" forHTTPHeaderField:@"Accept"];
	if (body && body[0] && ![verb isEqualToString:@"GET"] && ![verb isEqualToString:@"HEAD"]) {
		request.HTTPBody = [ns_string(body) dataUsingEncoding:NSUTF8StringEncoding];
	}

	start = CFAbsoluteTimeGetCurrent();
	[[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:^(NSData* data, NSURLResponse* response, NSError* error) {
		HttpResult* r = (HttpResult*)calloc(1, sizeof(HttpResult));
		NSHTTPURLResponse* http = [response isKindOfClass:[NSHTTPURLResponse class]] ? (NSHTTPURLResponse*)response : nil;
		r->callback = callback;
		r->user_data = user_data;
		r->ms = (CFAbsoluteTimeGetCurrent() - start) * 1000.0;
		if (error) {
			r->error = dup_ns(error.localizedDescription);
		} else {
			NSMutableString* headers = [NSMutableString string];
			NSString* type = @"";
			r->status = (int)http.statusCode;
			for (NSString* key in [http.allHeaderFields.allKeys sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]) {
				[headers appendFormat:@"%@: %@\n", key, http.allHeaderFields[key]];
				if ([key caseInsensitiveCompare:@"Content-Type"] == NSOrderedSame) type = http.allHeaderFields[key];
			}
			r->headers = dup_ns(headers);
			if (data.length == 0) {
				r->body = strdup("");
			} else if ([type rangeOfString:@"json" options:NSCaseInsensitiveSearch].location == NSNotFound) {
				/* JSON only: anything else is refused, with a glimpse of what came */
				NSString* raw = [[NSString alloc] initWithData:[data subdataWithRange:NSMakeRange(0, MIN(data.length, (NSUInteger)400))] encoding:NSUTF8StringEncoding];
				r->error = dup_ns([NSString stringWithFormat:@"Refused: the answer is not JSON (Content-Type: %@).\n\n%@%@",
					type.length ? type : @"none", raw ?: @"(binary)", data.length > 400 ? @"\n..." : @""]);
			} else {
				NSError* parse_error = nil;
				id json = [NSJSONSerialization JSONObjectWithData:data options:NSJSONReadingFragmentsAllowed error:&parse_error];
				if (!json) {
					r->error = dup_ns([NSString stringWithFormat:@"The answer says JSON but does not parse: %@", parse_error.localizedDescription]);
				} else {
					NSJSONWritingOptions options = NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys | NSJSONWritingFragmentsAllowed;
					if (@available(macOS 10.15, *)) options |= NSJSONWritingWithoutEscapingSlashes;
					r->body = dup_ns([[NSString alloc] initWithData:[NSJSONSerialization dataWithJSONObject:json options:options error:nil]
						encoding:NSUTF8StringEncoding]);
				}
			}
		}
		Rtask_post(http_on_ui, r);
	}] resume];
	return R_TRUE;
}
