// -*- Mode: objc; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*-
//
// This file is part of the LibreOffice project.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#import "config.h"

#import <cassert>
#import <cstdlib>
#import <cstring>

#import <LibreOfficeKit/LibreOfficeKit.hxx>

#define LIBO_INTERNAL_ONLY
#include <comphelper/lok.hxx>
#include <i18nlangtag/languagetag.hxx>

#import "ios.h"
#import "AppDelegate.h"
#import "DocumentBrowserViewController.h"
#import "DocumentViewController.h"
#import "Document.h"

#import "FakeSocket.hpp"
#import "Log.hpp"
#import "LOOLWSD.hpp"
#import "Util.hpp"

static LOOLWSD *loolwsd = nullptr;

NSString *app_locale;
BOOL empty_tile_cache;

static void download(NSURL *source, NSURL *destination) {
    [[[NSURLSession sharedSession] downloadTaskWithURL:source
                                     completionHandler:^(NSURL *location, NSURLResponse *response, NSError *error) {
                if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]] && [(NSHTTPURLResponse*)response statusCode] == 200) {
                    NSError *error = nil;
                    NSURL *resultingItem = nil;
                    if ([[NSFileManager defaultManager] replaceItemAtURL:destination
                                                           withItemAtURL:location
                                                          backupItemName:nil
                                                                 options:NSFileManagerItemReplacementUsingNewMetadataOnly
                                                        resultingItemURL:&resultingItem
                                                                error:&error]) {
                        LOG_INF("Downloaded " <<
                                [[source absoluteString] UTF8String] <<
                                " as " << [[destination absoluteString] UTF8String]);
                    } else {
                        LOG_ERR("Failed to replace " <<
                                [[destination absoluteString] UTF8String] <<
                                " with " << [[location absoluteString] UTF8String] <<
                                ": " << [[error description] UTF8String]);
                    }
                } else if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]]) {
                    LOG_ERR("Failed to download " <<
                            [[source absoluteString] UTF8String] <<
                            ": response code " << [(NSHTTPURLResponse*)response statusCode]);
                } else if (error != nil) {
                    LOG_ERR("Failed to download " <<
                            [[source absoluteString] UTF8String] <<
                            ": " << [[error description] UTF8String]);
                } else {
                    LOG_ERR("Failed to download " <<
                            [[source absoluteString] UTF8String]);
                }
            }] resume];
}

static void downloadTemplate(NSURL *source, NSURL *destination) {
    download(source, destination);
    // Download also a thumbnail
    download([NSURL URLWithString:[[source absoluteString] stringByAppendingString:@".png"]],
             [NSURL URLWithString:[[destination absoluteString] stringByAppendingString:@".png"]]);
}

static void updateTemplates(NSData *data, NSURLResponse *response)
{
    static NSString *downloadedTemplates = [[NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex:0] stringByAppendingString:@"/downloadedTemplates/"];

    // The data downloaded is a template list file, and should have one URL per line, corresponding
    // to a template document. For each URL, we check whether we have the corresponding template
    // document, and whether it is the same version (timestamp is older or the same). If not, we
    // download the new version. Finally we remove any earlier downloaded templates not mentioned in the list
    // file.

    NSMutableSet<NSString *> *urlHashes = [NSMutableSet setWithCapacity:10];

    const char *p = static_cast<const char*>([data bytes]);
    const char *endOfData = p + [data length];
    while (p < endOfData) {
        const char *endOfLine = static_cast<const char*>(std::memchr(p, '\n', [data length]));
        if (endOfLine == NULL)
            endOfLine = endOfData;

        // Allow comment lines staring with a hash sign.
        if (*p != '#') {
            const int length = endOfLine - p;
            // Allow empty lines
            if (length > 0) {
                std::vector<char> buf(length+1);
                std::memcpy(buf.data(), p, length);
                buf[length] = 0;

                NSString *line = [NSString stringWithUTF8String:buf.data()];

                NSURL *url = [NSURL URLWithString:line];
                if (url == nil)
                    LOG_ERR("Invalid URL in template file: " << [line UTF8String]);
                else {
                    NSString *baseName = [url lastPathComponent];

                    NSString *hash = [[NSData dataWithBytes:buf.data() length:length] base64EncodedStringWithOptions:0];
                    [urlHashes addObject:hash];

                    NSString *directoryForTemplate = [downloadedTemplates stringByAppendingString:hash];

                    NSURL *fileForTemplate = [NSURL fileURLWithPath:[directoryForTemplate stringByAppendingString:[@"/" stringByAppendingString:baseName]]];

                    // If we have that template, check whether it is up-to-date
                    BOOL isDirectory;
                    if ([[NSFileManager defaultManager] fileExistsAtPath:directoryForTemplate isDirectory:&isDirectory] &&
                        isDirectory) {
                        NSMutableURLRequest *req = [[NSURLRequest requestWithURL:url] mutableCopy];
                        [req setHTTPMethod:@"HEAD"];
                        [[[NSURLSession sharedSession] dataTaskWithRequest:req
                                                         completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                                    if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]] && [(NSHTTPURLResponse*)response statusCode] == 200) {
                                        NSString *lastModified = [[(NSHTTPURLResponse*)response allHeaderFields] objectForKey:@"Last-Modified"];
                                        NSDateFormatter *df = [[NSDateFormatter alloc] init];
                                        df.dateFormat = @"EEE, dd MMM yyyy HH:mm:ss z";
                                        NSDate *templateDate = [df dateFromString:lastModified];

                                        NSDate *cachedTemplateDate = [[[NSFileManager defaultManager] attributesOfItemAtPath:[fileForTemplate path] error:nil] objectForKey:NSFileModificationDate];

                                        LOG_INF("Template at " << [[url absoluteString] UTF8String] << " timestamp: "
                                                << [[templateDate descriptionWithLocale:nil] UTF8String] << ", cached template timestamp: "
                                                << [[cachedTemplateDate descriptionWithLocale:nil] UTF8String]);

                                        if ([templateDate compare:cachedTemplateDate] == NSOrderedDescending) {
                                            downloadTemplate(url, fileForTemplate);
                                        }
                                    } else if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]]) {
                                        LOG_ERR("Failed to get HEAD of " <<
                                                [[url absoluteString] UTF8String] <<
                                                ": response code " << [(NSHTTPURLResponse*)response statusCode]);
                                    } else if (error != nil) {
                                        LOG_ERR("Failed to get HEAD of " <<
                                                [[url absoluteString] UTF8String] <<
                                                ": " << [[error description] UTF8String]);
                                    } else {
                                        LOG_ERR("Failed to get HEAD of " <<
                                                [[url absoluteString] UTF8String]);
                                    }
                                }] resume];
                    } else {
                        // Else download it.
                        [[NSFileManager defaultManager] createDirectoryAtPath:directoryForTemplate withIntermediateDirectories:YES attributes:nil error:nil];
                        downloadTemplate(url, fileForTemplate);
                    }
                }
            }
        }
        if (endOfLine < endOfData)
            p = endOfLine + 1;
        else
            p = endOfData;
    }

    // Remove templates that are no longer mentioned in the list file.
    NSArray<NSString *> *dirContents = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:downloadedTemplates error:nil];
    for (int i = 0; i < [dirContents count]; i++) {
        if (![urlHashes containsObject:[dirContents objectAtIndex:i]]) {
            [[NSFileManager defaultManager] removeItemAtPath:[downloadedTemplates stringByAppendingString:[@"/" stringByAppendingString:[dirContents objectAtIndex:i]]]
                                                       error:nil];
        }
    }
}

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    auto trace = std::getenv("LOOL_LOGLEVEL");
    if (!trace)
        trace = strdup("warning");

    setenv("SAL_LOK_OPTIONS", "unipoll", 0);

    Log::initialize("Mobile", trace, false, false, {});
    Util::setThreadName("main");

    // Having LANG in the environment is expected to happen only when debugging from Xcode. When
    // testing some language one doesn't know it might be risky to simply set one's iPad to that
    // language, as it might be hard to find the way to set it back to a known language.

    char *lang = std::getenv("LANG");
    if (lang != nullptr)
        app_locale = [NSString stringWithUTF8String:lang];
    else
        app_locale = [[NSLocale preferredLanguages] firstObject];

    lo_kit = lok_init_2(nullptr, nullptr);

    comphelper::LibreOfficeKit::setLanguageTag(LanguageTag(OUString::fromUtf8(OString([app_locale UTF8String])), true));

    // Look for the setting indicating the URL for a file containing a list of URLs for template
    // documents to download. If set, start a task to download it, and then to download the listed
    // templates.

    NSString *templateListURL = nil;

    // First check managed configuration, if present
    NSDictionary *managedConfig = [[NSUserDefaults standardUserDefaults] dictionaryForKey:@"com.apple.configuration.managed"];
    if (managedConfig != nil) {
        templateListURL = managedConfig[@"templateListURL"];
        if (templateListURL != nil && ![templateListURL isKindOfClass:[NSString class]])
            templateListURL = nil;
    }

    if (templateListURL == nil)
        templateListURL = [[NSUserDefaults standardUserDefaults] stringForKey:@"templateListURL"];

    if (templateListURL != nil) {
        NSURL *url = [NSURL URLWithString:templateListURL];
        if (url != nil) {
            [[[NSURLSession sharedSession] dataTaskWithURL:url
                                         completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
                        if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]] && [(NSHTTPURLResponse*)response statusCode] == 200)
                            updateTemplates(data, response);
                        else if (error == nil && [response isKindOfClass:[NSHTTPURLResponse class]])
                            LOG_ERR("Failed to download " <<
                                    [[url absoluteString] UTF8String] <<
                                    ": response code " << [(NSHTTPURLResponse*)response statusCode]);
                        else if (error != nil)
                            LOG_ERR("Failed to download " <<
                                    [[url absoluteString] UTF8String] <<
                                    ": " << [[error description] UTF8String]);
                        else
                            LOG_ERR("Failed to download " <<
                                    [[url absoluteString] UTF8String]);
                    }] resume];
        }
    }

    fakeSocketSetLoggingCallback([](const std::string& line)
                                 {
                                     LOG_TRC_NOFILE(line);
                                 });

    dispatch_async(dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                   ^{
                       assert(loolwsd == nullptr);
                       char *argv[2];
                       argv[0] = strdup([[NSBundle mainBundle].executablePath UTF8String]);
                       argv[1] = nullptr;
                       Util::setThreadName("app");
                       while (true) {
                           loolwsd = new LOOLWSD();
                           loolwsd->run(1, argv);
                           delete loolwsd;
                           LOG_TRC("One run of LOOLWSD completed");
                       }
                   });
    return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
}

- (void)applicationDidEnterBackground:(UIApplication *)application {
}

- (void)applicationWillEnterForeground:(UIApplication *)application {
}

- (void)applicationDidBecomeActive:(UIApplication *)application {
}

- (void)applicationWillTerminate:(UIApplication *)application {
    // tdf#126974 We don't want any global object destructors to be called, the code
    // is not prepared for that.
    std::_Exit(1);
}

- (BOOL)application:(UIApplication *)app openURL:(NSURL *)inputURL options:(NSDictionary<UIApplicationOpenURLOptionsKey, id> *)options {
    // Ensure the URL is a file URL
    if (!inputURL.isFileURL) {
        return NO;
    }

    // If we already have a document open, close it
    if (lok_document != nullptr && [DocumentViewController singleton] != nil) {
        [[DocumentViewController singleton] bye];
    }

    // Reveal / import the document at the URL
    DocumentBrowserViewController *documentBrowserViewController = (DocumentBrowserViewController *)self.window.rootViewController;
    [documentBrowserViewController revealDocumentAtURL:inputURL importIfNeeded:YES completion:^(NSURL * _Nullable revealedDocumentURL, NSError * _Nullable error) {
        if (error) {
            LOG_ERR("Failed to reveal the document at URL " << [[inputURL description] UTF8String] << " with error: " << [[error description] UTF8String]);
            return;
        }

        // Present the Document View Controller for the revealed URL
        [documentBrowserViewController presentDocumentAtURL:revealedDocumentURL];
    }];
    return YES;
}

@end

// vim:set shiftwidth=4 softtabstop=4 expandtab:
