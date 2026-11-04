#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface RaineyAppDelegate : NSObject <NSApplicationDelegate, WKScriptMessageHandler>
@property(nonatomic, strong) NSWindow *window;
@property(nonatomic, strong) WKWebView *webView;
@end

@implementation RaineyAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    WKUserContentController *contentController = [WKUserContentController new];
    [contentController addScriptMessageHandler:self name:@"rainey"];

    WKWebViewConfiguration *configuration = [WKWebViewConfiguration new];
    configuration.userContentController = contentController;
    configuration.preferences.javaScriptCanOpenWindowsAutomatically = NO;

    self.webView = [[WKWebView alloc] initWithFrame:NSZeroRect configuration:configuration];
    [self.webView setValue:@NO forKey:@"drawsBackground"];

    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1280, 720)
                                             styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable | NSWindowStyleMaskFullSizeContentView
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    self.window.title = @"Rainey";
    self.window.minSize = NSMakeSize(960, 540);
    self.window.contentView = self.webView;
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];

    NSURL *url = [[NSBundle mainBundle] URLForResource:@"index" withExtension:@"html"];
    NSURL *resourceURL = [[NSBundle mainBundle] resourceURL] ?: [url URLByDeletingLastPathComponent];
    if (url != nil) {
        [self.webView loadFileURL:url allowingReadAccessToURL:resourceURL];
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    return YES;
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    if (![message.name isEqualToString:@"rainey"] || ![message.body isKindOfClass:[NSDictionary class]]) {
        return;
    }

    NSDictionary *body = (NSDictionary *)message.body;
    NSString *action = body[@"action"];
    if (![action isKindOfClass:[NSString class]]) {
        return;
    }

    if ([action isEqualToString:@"pickFolder"]) {
        NSString *systemId = body[@"systemId"];
        NSArray *extensions = body[@"extensions"];
        if ([systemId isKindOfClass:[NSString class]] && [extensions isKindOfClass:[NSArray class]]) {
            [self pickFolderForSystem:systemId extensions:extensions];
        }
    } else if ([action isEqualToString:@"pickFiles"]) {
        NSString *systemId = body[@"systemId"];
        NSArray *extensions = body[@"extensions"];
        if ([systemId isKindOfClass:[NSString class]] && [extensions isKindOfClass:[NSArray class]]) {
            [self pickFilesForSystem:systemId extensions:extensions];
        }
    } else if ([action isEqualToString:@"launch"]) {
        NSString *path = body[@"path"];
        if ([path isKindOfClass:[NSString class]]) {
            [self launchPath:path];
        }
    } else if ([action isEqualToString:@"chooseRetroArchApp"]) {
        [self chooseRetroArchApp];
    } else if ([action isEqualToString:@"chooseRetroArchCore"]) {
        NSString *systemId = body[@"systemId"];
        if ([systemId isKindOfClass:[NSString class]]) {
            [self chooseRetroArchCoreForSystem:systemId];
        }
    } else if ([action isEqualToString:@"launchRetroArch"]) {
        NSString *romPath = body[@"romPath"];
        NSString *appPath = body[@"appPath"];
        NSString *coreSetting = body[@"coreSetting"];
        BOOL allowFallback = [body[@"allowFallback"] boolValue];
        if ([romPath isKindOfClass:[NSString class]]) {
            [self launchRetroArchROM:romPath appPath:appPath coreSetting:coreSetting allowFallback:allowFallback];
        }
    } else if ([action isEqualToString:@"saveLibrary"]) {
        NSString *payload = body[@"payload"];
        if ([payload isKindOfClass:[NSString class]]) {
            [self saveLibrary:payload];
        }
    } else if ([action isEqualToString:@"loadLibrary"]) {
        [self loadLibrary];
    } else if ([action isEqualToString:@"showInFinder"]) {
        NSString *path = body[@"path"];
        if ([path isKindOfClass:[NSString class]]) {
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[[NSURL fileURLWithPath:path]]];
        }
    }
}

- (void)chooseRetroArchApp {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Choose RetroArch";
    panel.prompt = @"Use RetroArch";
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = @[@"app"];
#pragma clang diagnostic pop

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK && panel.URL != nil) {
            [self callJavaScript:@"window.raineyNative.onRetroArchAppPicked" argument:@{
                @"path": panel.URL.path ?: @""
            }];
        } else {
            [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
                @"kind": @"info",
                @"message": @"RetroArch app selection cancelled."
            }];
        }
    }];
}

- (void)chooseRetroArchCoreForSystem:(NSString *)systemId {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Choose RetroArch Core";
    panel.prompt = @"Use Core";
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = @[@"dylib", @"so"];
#pragma clang diagnostic pop

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK && panel.URL != nil) {
            [self callJavaScript:@"window.raineyNative.onRetroArchCorePicked" argument:@{
                @"systemId": systemId,
                @"path": panel.URL.path ?: @""
            }];
        } else {
            [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
                @"kind": @"info",
                @"message": @"RetroArch core selection cancelled."
            }];
        }
    }];
}

- (void)pickFilesForSystem:(NSString *)systemId extensions:(NSArray *)extensions {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Load Content";
    panel.prompt = @"Add";
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = YES;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    panel.allowedFileTypes = extensions;
#pragma clang diagnostic pop

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK && panel.URLs.count > 0) {
            NSArray *games = [self gamesForURLs:panel.URLs systemId:systemId allowedExtensions:extensions];
            [self callJavaScript:@"window.raineyNative.onContentLoaded" argument:@{
                @"systemId": systemId,
                @"games": games
            }];
        } else {
            [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
                @"kind": @"info",
                @"message": @"Content selection cancelled."
            }];
        }
    }];
}

- (void)pickFolderForSystem:(NSString *)systemId extensions:(NSArray *)extensions {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Choose ROM Folder";
    panel.prompt = @"Scan";
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;

    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK && panel.URL != nil) {
            NSArray *games = [self scanFolder:panel.URL systemId:systemId extensions:extensions];
            NSDictionary *folder = @{
                @"path": panel.URL.path ?: @"",
                @"name": panel.URL.lastPathComponent ?: @"ROM Folder"
            };
            [self callJavaScript:@"window.raineyNative.onScanResult" argument:@{
                @"systemId": systemId,
                @"folder": folder,
                @"games": games
            }];
        } else {
            [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
                @"kind": @"info",
                @"message": @"Folder selection cancelled."
            }];
        }
    }];
}

- (NSArray *)scanFolder:(NSURL *)folder systemId:(NSString *)systemId extensions:(NSArray *)extensions {
    NSMutableSet *normalized = [NSMutableSet set];
    for (id item in extensions) {
        if ([item isKindOfClass:[NSString class]]) {
            NSString *extension = [[(NSString *)item lowercaseString] stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"."]];
            [normalized addObject:extension];
        }
    }

    NSArray *keys = @[NSURLIsDirectoryKey, NSURLIsRegularFileKey, NSURLLocalizedNameKey];
    NSDirectoryEnumerator *enumerator = [[NSFileManager defaultManager] enumeratorAtURL:folder
                                                            includingPropertiesForKeys:keys
                                                                               options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                          errorHandler:nil];
    NSMutableArray *matchingURLs = [NSMutableArray array];

    for (NSURL *url in enumerator) {
        NSNumber *isDirectory = nil;
        NSNumber *isRegularFile = nil;
        [url getResourceValue:&isDirectory forKey:NSURLIsDirectoryKey error:nil];
        [url getResourceValue:&isRegularFile forKey:NSURLIsRegularFileKey error:nil];
        if (isDirectory.boolValue || !isRegularFile.boolValue) {
            continue;
        }

        NSString *extension = url.pathExtension.lowercaseString ?: @"";
        if (![normalized containsObject:extension]) {
            continue;
        }

        [matchingURLs addObject:url];
    }

    return [self gamesForURLs:matchingURLs systemId:systemId allowedExtensions:extensions];
}

- (NSArray *)gamesForURLs:(NSArray *)urls systemId:(NSString *)systemId allowedExtensions:(NSArray *)extensions {
    NSMutableSet *normalized = [NSMutableSet set];
    for (id item in extensions) {
        if ([item isKindOfClass:[NSString class]]) {
            NSString *extension = [[(NSString *)item lowercaseString] stringByTrimmingCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@"."]];
            [normalized addObject:extension];
        }
    }

    NSMutableArray *games = [NSMutableArray array];
    NSMutableSet *playStationCueBases = [NSMutableSet set];
    if ([systemId isEqualToString:@"playstation"]) {
        for (NSURL *url in urls) {
            if ([url isKindOfClass:[NSURL class]] && [url.pathExtension.lowercaseString isEqualToString:@"cue"]) {
                [playStationCueBases addObject:[url URLByDeletingPathExtension].path];
            }
        }
    }

    for (NSURL *url in urls) {
        if (![url isKindOfClass:[NSURL class]]) {
            continue;
        }
        NSString *extension = url.pathExtension.lowercaseString ?: @"";
        if (![normalized containsObject:extension]) {
            continue;
        }
        if ([systemId isEqualToString:@"playstation"] && [extension isEqualToString:@"bin"] && [playStationCueBases containsObject:[url URLByDeletingPathExtension].path]) {
            continue;
        }

        NSString *title = [[url URLByDeletingPathExtension].lastPathComponent stringByReplacingOccurrencesOfString:@"_" withString:@" "];
        title = [title stringByReplacingOccurrencesOfString:@"." withString:@" "];
        title = [title stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (title.length == 0) {
            title = url.lastPathComponent ?: @"Game";
        }

        NSNumber *pathHash = @((NSInteger)url.path.hash);
        [games addObject:@{
            @"id": [NSString stringWithFormat:@"%@-%@", systemId, pathHash],
            @"system": systemId,
            @"title": title,
            @"ext": extension,
            @"path": url.path ?: @"",
            @"favorite": @NO,
            @"recent": @NO,
            @"year": @"Unknown",
            @"genre": @"Unknown",
            @"developer": @"Unknown"
        }];
    }

    [games sortUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"title" ascending:YES selector:@selector(localizedCaseInsensitiveCompare:)]]];
    return games;
}

- (void)launchPath:(NSString *)path {
    if (![[NSFileManager defaultManager] fileExistsAtPath:path]) {
        [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
            @"kind": @"error",
            @"message": @"This game file no longer exists. Re-scan the folder or choose the file again."
        }];
        return;
    }

    BOOL opened = [[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:path]];
    [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
        @"kind": opened ? @"success" : @"error",
        @"message": opened
            ? @"Opening game with the default macOS app for this file type."
            : @"macOS could not find an app for this game. Install an emulator and associate this file type."
    }];
}

- (void)launchRetroArchROM:(NSString *)romPath appPath:(NSString *)appPath coreSetting:(NSString *)coreSetting allowFallback:(BOOL)allowFallback {
    if (![[NSFileManager defaultManager] fileExistsAtPath:romPath]) {
        [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
            @"kind": @"error",
            @"message": @"This game file no longer exists. Re-scan the folder or choose the file again."
        }];
        return;
    }

    NSString *retroArchExecutable = [self retroArchExecutableFromPath:appPath];
    if (retroArchExecutable.length == 0 || ![[NSFileManager defaultManager] isExecutableFileAtPath:retroArchExecutable]) {
        [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
            @"kind": allowFallback ? @"info" : @"error",
            @"message": allowFallback
                ? @"RetroArch is not configured or could not be found. Falling back to the default macOS launcher."
                : @"RetroArch is not configured. Choose RetroArch.app in Settings or install RetroArch for macOS."
        }];
        if (allowFallback) {
            [self launchPath:romPath];
        }
        return;
    }

    NSString *corePath = [self resolvedCorePath:coreSetting appPath:appPath];
    if (corePath.length == 0 || ![[NSFileManager defaultManager] fileExistsAtPath:corePath]) {
        [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
            @"kind": allowFallback ? @"info" : @"error",
            @"message": allowFallback
                ? @"RetroArch core is missing for this system. Falling back to the default macOS launcher."
                : @"RetroArch core is missing for this system. Choose a .dylib core in Settings."
        }];
        if (allowFallback) {
            [self launchPath:romPath];
        }
        return;
    }

    NSError *error = nil;
    BOOL launched = NO;
    if (appPath.length > 0 && [appPath.pathExtension.lowercaseString isEqualToString:@"app"] && [[NSFileManager defaultManager] fileExistsAtPath:appPath]) {
        NSWorkspaceOpenConfiguration *configuration = [NSWorkspaceOpenConfiguration configuration];
        configuration.arguments = @[@"-L", corePath, romPath];
        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
        __block NSError *workspaceError = nil;
        [[NSWorkspace sharedWorkspace] openApplicationAtURL:[NSURL fileURLWithPath:appPath]
                                              configuration:configuration
                                          completionHandler:^(NSRunningApplication *app, NSError *openError) {
            workspaceError = openError;
            dispatch_semaphore_signal(semaphore);
        }];
        dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
        error = workspaceError;
        launched = (workspaceError == nil);
    } else {
        NSTask *task = [NSTask new];
        task.executableURL = [NSURL fileURLWithPath:retroArchExecutable];
        task.arguments = @[@"-L", corePath, romPath];
        launched = [task launchAndReturnError:&error];
    }
    [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
        @"kind": launched ? @"success" : @"error",
        @"message": launched
            ? @"Launching with RetroArch."
            : [NSString stringWithFormat:@"RetroArch could not start: %@", error.localizedDescription ?: @"Unknown error"]
    }];
    if (!launched && allowFallback) {
        [self launchPath:romPath];
    }
}

- (NSString *)retroArchExecutableFromPath:(NSString *)path {
    NSString *candidate = path;
    if (candidate.length == 0) {
        candidate = @"/Applications/RetroArch.app";
    }

    BOOL isDirectory = NO;
    if ([[NSFileManager defaultManager] fileExistsAtPath:candidate isDirectory:&isDirectory] && isDirectory && [candidate.pathExtension.lowercaseString isEqualToString:@"app"]) {
        NSString *bundleExecutable = [candidate stringByAppendingPathComponent:@"Contents/MacOS/RetroArch"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:bundleExecutable]) {
            return bundleExecutable;
        }
    }

    if ([[NSFileManager defaultManager] fileExistsAtPath:candidate]) {
        return candidate;
    }

    return @"";
}

- (NSString *)resolvedCorePath:(NSString *)coreSetting appPath:(NSString *)appPath {
    if (coreSetting.length == 0) {
        return @"";
    }

    NSArray *parts = [coreSetting componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@";,\n"]];
    for (NSString *part in parts) {
        NSString *resolved = [self resolvedSingleCorePath:[part stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] appPath:appPath];
        if (resolved.length > 0 && [[NSFileManager defaultManager] fileExistsAtPath:resolved]) {
            return resolved;
        }
    }

    return [self resolvedSingleCorePath:[[parts firstObject] ?: coreSetting stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] appPath:appPath];
}

- (NSString *)resolvedSingleCorePath:(NSString *)coreSetting appPath:(NSString *)appPath {
    if (coreSetting.length == 0) {
        return @"";
    }
    NSString *expanded = [coreSetting stringByExpandingTildeInPath];
    if (expanded.isAbsolutePath && [[NSFileManager defaultManager] fileExistsAtPath:expanded]) {
        return expanded;
    }

    NSMutableArray *names = [NSMutableArray arrayWithObject:coreSetting.lastPathComponent ?: coreSetting];
    if (![coreSetting.pathExtension.lowercaseString isEqualToString:@"dylib"]) {
        [names addObject:[coreSetting stringByAppendingString:@"_libretro.dylib"]];
        [names addObject:[coreSetting stringByAppendingString:@".dylib"]];
    }

    NSMutableArray *roots = [NSMutableArray array];
    [roots addObject:[@"~/Library/Application Support/RetroArch/cores" stringByExpandingTildeInPath]];
    [roots addObject:@"/Library/Application Support/RetroArch/cores"];

    if (appPath.length > 0 && [appPath.pathExtension.lowercaseString isEqualToString:@"app"]) {
        [roots addObject:[appPath stringByAppendingPathComponent:@"Contents/Resources/cores"]];
    }
    [roots addObject:@"/Applications/RetroArch.app/Contents/Resources/cores"];

    for (NSString *root in roots) {
        for (NSString *name in names) {
            NSString *candidate = [root stringByAppendingPathComponent:name];
            if ([[NSFileManager defaultManager] fileExistsAtPath:candidate]) {
                return candidate;
            }
        }
    }

    return expanded;
}

- (void)saveLibrary:(NSString *)payload {
    NSError *error = nil;
    NSURL *url = [self libraryURL:&error];
    if (url == nil || error != nil) {
        [self showSaveError:error];
        return;
    }
    [[NSFileManager defaultManager] createDirectoryAtURL:url.URLByDeletingLastPathComponent withIntermediateDirectories:YES attributes:nil error:&error];
    if (error == nil) {
        [payload writeToURL:url atomically:YES encoding:NSUTF8StringEncoding error:&error];
    }
    if (error != nil) {
        [self showSaveError:error];
    }
}

- (void)showSaveError:(NSError *)error {
    [self callJavaScript:@"window.raineyNative.onMessage" argument:@{
        @"kind": @"error",
        @"message": [NSString stringWithFormat:@"Could not save the desktop library: %@", error.localizedDescription ?: @"Unknown error"]
    }];
}

- (void)loadLibrary {
    NSError *error = nil;
    NSURL *url = [self libraryURL:&error];
    NSString *payload = nil;
    if (url != nil) {
        payload = [NSString stringWithContentsOfURL:url encoding:NSUTF8StringEncoding error:nil];
    }
    [self callJavaScript:@"window.raineyNative.onLibraryLoaded" rawJSON:payload ?: @"{}"];
}

- (NSURL *)libraryURL:(NSError **)error {
    NSURL *base = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
                                                        inDomain:NSUserDomainMask
                                               appropriateForURL:nil
                                                          create:YES
                                                           error:error];
    return [[base URLByAppendingPathComponent:@"Rainey Desktop" isDirectory:YES] URLByAppendingPathComponent:@"library.json"];
}

- (void)callJavaScript:(NSString *)function argument:(id)argument {
    if (![NSJSONSerialization isValidJSONObject:argument]) {
        return;
    }
    NSData *data = [NSJSONSerialization dataWithJSONObject:argument options:0 error:nil];
    NSString *json = [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
    [self callJavaScript:function rawJSON:json ?: @"{}"];
}

- (void)callJavaScript:(NSString *)function rawJSON:(NSString *)rawJSON {
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.webView evaluateJavaScript:[NSString stringWithFormat:@"%@(%@);", function, rawJSON] completionHandler:nil];
    });
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        RaineyAppDelegate *delegate = [RaineyAppDelegate new];
        app.delegate = delegate;
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        [app activateIgnoringOtherApps:YES];
        [app run];
    }
    return 0;
}
