#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include "unmad_fs_core.h"
#include "gui_assets.h"
#include <iostream>

@interface ScriptBridge : NSObject <WKScriptMessageHandler>
@property (nonatomic, weak) WKWebView *webView;
@end

@implementation ScriptBridge

- (void)userContentController:(WKUserContentController *)userContentController
      didReceiveScriptMessage:(WKScriptMessage *)message {
    (void)userContentController;
    if (![message.body isKindOfClass:[NSDictionary class]]) return;
    NSDictionary *dict = (NSDictionary *)message.body;
    NSString *action = dict[@"action"];

    if ([action isEqualToString:@"cmd"]) {
        NSString *cmd = dict[@"command"];
        char outBuf[16384] = {0};
        fs_execute_command([cmd UTF8String], outBuf, sizeof(outBuf) - 1);

        char stateJson[65536] = {0};
        fs_get_state_json(stateJson, sizeof(stateJson) - 1);

        NSString *outStr = [NSString stringWithUTF8String:outBuf];
        NSData *outJsonData = [NSJSONSerialization dataWithJSONObject:@{@"output": outStr ? outStr : @""} options:0 error:nil];
        NSString *outJson = [[NSString alloc] initWithData:outJsonData encoding:NSUTF8StringEncoding];

        NSString *script = [NSString stringWithFormat:
            @"window.onBackendResult({ output: %@.output, state: %@ });",
            outJson, [NSString stringWithUTF8String:stateJson]];
        [self.webView evaluateJavaScript:script completionHandler:nil];

    } else if ([action isEqualToString:@"getState"]) {
        char stateJson[65536] = {0};
        fs_get_state_json(stateJson, sizeof(stateJson) - 1);
        NSString *script = [NSString stringWithFormat:
            @"window.onBackendResult({ state: %@ });",
            [NSString stringWithUTF8String:stateJson]];
        [self.webView evaluateJavaScript:script completionHandler:nil];

    } else if ([action isEqualToString:@"getHex"]) {
        NSNumber *blkNum = dict[@"block"];
        uint32_t blk = blkNum ? [blkNum unsignedIntValue] : 0;
        char hexBuf[16384] = {0};
        fs_get_block_hex(blk, hexBuf, sizeof(hexBuf) - 1);
        NSString *hexStr = [NSString stringWithUTF8String:hexBuf];
        NSData *hexJsonData = [NSJSONSerialization dataWithJSONObject:@{@"hex": hexStr ? hexStr : @""} options:0 error:nil];
        NSString *hexJson = [[NSString alloc] initWithData:hexJsonData encoding:NSUTF8StringEncoding];

        NSString *script = [NSString stringWithFormat:
            @"window.onBackendResult({ hex: %@.hex });", hexJson];
        [self.webView evaluateJavaScript:script completionHandler:nil];
    }
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@property (nonatomic, strong) NSWindow *window;
@property (nonatomic, strong) WKWebView *webView;
@property (nonatomic, strong) ScriptBridge *bridge;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    (void)notification;
    // 1. Determine persistent on-device location for disk.img
    NSString *diskPath = @"disk.img";
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) && strcmp(cwd, "/") == 0) {
        NSString *bundlePath = [[NSBundle mainBundle] bundlePath];
        NSString *parentDir = [bundlePath stringByDeletingLastPathComponent];
        diskPath = [parentDir stringByAppendingPathComponent:@"disk.img"];
    }

    if (fs_init([diskPath UTF8String]) != 0) {
        NSLog(@"Failed to initialize disk.img at %@", diskPath);
    }

    // 2. Setup WebKit configuration with message handler
    WKWebViewConfiguration *config = [[WKWebViewConfiguration alloc] init];
    WKUserContentController *ucc = [[WKUserContentController alloc] init];
    self.bridge = [[ScriptBridge alloc] init];
    [ucc addScriptMessageHandler:self.bridge name:@"unmadFs"];
    config.userContentController = ucc;

    // 3. Create Window
    NSRect frame = NSMakeRect(100, 100, 1180, 780);
    self.window = [[NSWindow alloc] initWithContentRect:frame
                                             styleMask:(NSWindowStyleMaskTitled |
                                                        NSWindowStyleMaskClosable |
                                                        NSWindowStyleMaskMiniaturizable |
                                                        NSWindowStyleMaskResizable)
                                               backing:NSBackingStoreBuffered
                                                 defer:NO];
    [self.window setTitle:@"Unmad Mini File System OS — Desktop GUI"];
    [self.window setMinSize:NSMakeSize(900, 600)];
    self.window.delegate = self;

    // 4. Create and attach WebView
    self.webView = [[WKWebView alloc] initWithFrame:self.window.contentView.bounds configuration:config];
    self.webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.bridge.webView = self.webView;
    [self.window.contentView addSubview:self.webView];

    // 5. Load embedded GUI HTML
    NSString *html = [NSString stringWithUTF8String:kGuiHTML];
    [self.webView loadHTMLString:html baseURL:nil];

    // 6. Show Window
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];
}

- (void)windowWillClose:(NSNotification *)notification {
    (void)notification;
    fs_close();
    [NSApp terminate:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
    (void)sender;
    return YES;
}

@end

int main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        AppDelegate *delegate = [[AppDelegate alloc] init];
        app.delegate = delegate;

        // Application menu
        NSMenu *menubar = [[NSMenu alloc] init];
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [app setMainMenu:menubar];

        NSMenu *appMenu = [[NSMenu alloc] init];
        NSString *appName = @"Unmad FS";
        NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:[@"Quit " stringByAppendingString:appName]
                                                              action:@selector(terminate:)
                                                       keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];

        [app run];
    }
    return 0;
}
