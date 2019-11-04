// -*- Mode: ObjC; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*-
//
// This file is part of the LibreOffice project.
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#import "config.h"

#import <cstdio>
#import <string>
#import <vector>

#import <poll.h>

#import "ios.h"
#import "FakeSocket.hpp"
#import "Log.hpp"
#import "SigUtil.hpp"
#import "Util.hpp"

#import "DocumentViewController.h"

static DocumentViewController* theSingleton = nil;

@interface DocumentViewController() <WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler, UIScrollViewDelegate> {
    int closeNotificationPipeForForwardingThread[2];
}

@end

@implementation DocumentViewController

- (void)viewDidLoad {
    [super viewDidLoad];

    theSingleton = self;

    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
    WKUserContentController *userContentController = [[WKUserContentController alloc] init];

    [userContentController addScriptMessageHandler:self name:@"debug"];
    [userContentController addScriptMessageHandler:self name:@"lool"];
    [userContentController addScriptMessageHandler:self name:@"error"];

    configuration.userContentController = userContentController;

    self.webView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];
    self.webView.translatesAutoresizingMaskIntoConstraints = NO;
    self.webView.allowsLinkPreview = NO;

    // Prevent the WebView from scrolling. Sadly I couldn't figure out how to do it in the JS,
    // so the problem is still there when using Online from Mobile Safari.
    self.webView.scrollView.scrollEnabled = NO;

    // Prevent the user from zooming the WebView by assigning ourselves as the delegate, and
    // stopping any zoom attempt in scrollViewWillBeginZooming: below. (The zooming of the document
    // contents is handled fully in JavaScript, the WebView has no knowledge of that.)
    self.webView.scrollView.delegate = self;

    [self.view addSubview:self.webView];

    self.webView.navigationDelegate = self;
    self.webView.UIDelegate = self;

    WKWebView *webViewP = self.webView;
    NSDictionary *views = NSDictionaryOfVariableBindings(webViewP);
    [self.view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-0-[webViewP(>=0)]-0-|"
                                                                      options:0
                                                                      metrics:nil
                                                                        views:views]];
    [self.view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|-0-[webViewP(>=0)]-0-|"
                                                                      options:0
                                                                      metrics:nil
                                                                        views:views]];
}

- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];

    // When the user uses the camer to insert a photo, when the camera is displayed, this view is
    // removed. After the photo is taken it is then added back to the hierarchy. Our Document object
    // is still there intact, however, so no need to re-open the document when we re-appear.

    // Check whether the Document object is an already initialised one.
    if (self.document->fakeClientFd >= 0)
        return;

    [self.document openWithCompletionHandler:^(BOOL success) {
        if (success) {
            // Display the content of the document
        } else {
            // Make sure to handle the failed import appropriately, e.g., by presenting an error message to the user.
        }
    }];
}

- (IBAction)dismissDocumentViewController {
    [self dismissViewControllerAnimated:YES completion:^ {
            [self.document closeWithCompletionHandler:^(BOOL success){
                    LOG_TRC("close completion handler gets " << (success?"YES":"NO"));
                    [self.webView.configuration.userContentController removeScriptMessageHandlerForName:@"debug"];
                    [self.webView.configuration.userContentController removeScriptMessageHandlerForName:@"lool"];
                    [self.webView.configuration.userContentController removeScriptMessageHandlerForName:@"error"];
                    self.webView.configuration.userContentController = nil;
                    [self.webView removeFromSuperview];
                    self.webView = nil;
                    }];
    }];
}

- (void)webView:(WKWebView *)webView didCommitNavigation:(WKNavigation *)navigation {
    LOG_TRC("didCommitNavigation: " << [[navigation description] UTF8String]);
}

- (void)webView:(WKWebView *)webView didFailNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    LOG_TRC("didFailNavigation: " << [[navigation description] UTF8String]);
}

- (void)webView:(WKWebView *)webView didFailProvisionalNavigation:(WKNavigation *)navigation withError:(NSError *)error {
    LOG_TRC("didFailProvisionalNavigation: " << [[navigation description] UTF8String]);
}

- (void)webView:(WKWebView *)webView didFinishNavigation:(WKNavigation *)navigation {
    LOG_TRC("didFinishNavigation: " << [[navigation description] UTF8String]);
}

- (void)webView:(WKWebView *)webView didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)navigation {
    LOG_TRC("didReceiveServerRedirectForProvisionalNavigation: " << [[navigation description] UTF8String]);
}

- (void)webView:(WKWebView *)webView didStartProvisionalNavigation:(WKNavigation *)navigation {
    LOG_TRC("didStartProvisionalNavigation: " << [[navigation description] UTF8String]);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationAction:(WKNavigationAction *)navigationAction decisionHandler:(void (^)(WKNavigationActionPolicy))decisionHandler {
    LOG_TRC("decidePolicyForNavigationAction: " << [[navigationAction description] UTF8String]);
    decisionHandler(WKNavigationActionPolicyAllow);
}

- (void)webView:(WKWebView *)webView decidePolicyForNavigationResponse:(WKNavigationResponse *)navigationResponse decisionHandler:(void (^)(WKNavigationResponsePolicy))decisionHandler {
    LOG_TRC("decidePolicyForNavigationResponse: " << [[navigationResponse description] UTF8String]);
    decisionHandler(WKNavigationResponsePolicyAllow);
}

- (WKWebView *)webView:(WKWebView *)webView createWebViewWithConfiguration:(WKWebViewConfiguration *)configuration forNavigationAction:(WKNavigationAction *)navigationAction windowFeatures:(WKWindowFeatures *)windowFeatures {
    LOG_TRC("createWebViewWithConfiguration");
    return webView;
}

- (void)webView:(WKWebView *)webView runJavaScriptAlertPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(void))completionHandler {
    LOG_TRC("runJavaScriptAlertPanelWithMessage: " << [message UTF8String]);
    //    UIAlertView *alert = [[UIAlertView alloc] initWithTitle:@""
    //                                                    message:message
    //                                                   delegate:nil
    //                                          cancelButtonTitle:nil
    //                                          otherButtonTitles:@"OK", nil];
    //    [alert show];
    completionHandler();
}

- (void)webView:(WKWebView *)webView runJavaScriptConfirmPanelWithMessage:(NSString *)message initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(BOOL result))completionHandler {
    LOG_TRC("runJavaScriptConfirmPanelWithMessage: " << [message UTF8String]);
    completionHandler(YES);
}

- (void)webView:(WKWebView *)webView runJavaScriptTextInputPanelWithPrompt:(NSString *)prompt defaultText:(NSString *)defaultText initiatedByFrame:(WKFrameInfo *)frame completionHandler:(void (^)(NSString *result))completionHandler {
    LOG_TRC("runJavaScriptTextInputPanelWithPrompt: " << [prompt UTF8String]);
    completionHandler(@"Something happened.");
}

- (void)userContentController:(WKUserContentController *)userContentController didReceiveScriptMessage:(WKScriptMessage *)message {
    int rc;
    struct pollfd p;

    if ([message.name isEqualToString:@"error"]) {
        LOG_ERR("Error from WebView: " << [message.body UTF8String]);
    } else if ([message.name isEqualToString:@"debug"]) {
        LOG_TRC_NOFILE("==> " << [message.body UTF8String]);
    } else if ([message.name isEqualToString:@"lool"]) {
        NSString *subBody = [message.body substringToIndex:std::min(100ul, ((NSString*)message.body).length)];
        if (subBody.length < ((NSString*)message.body).length)
            subBody = [subBody stringByAppendingString:@"..."];

        LOG_TRC("To Online: " << [subBody UTF8String]);

        if ([message.body isEqualToString:@"HULLO"]) {
            // Now we know that the JS has started completely

            // Contact the permanently (during app lifetime) listening LOOLWSD server
            // "public" socket
            assert(loolwsd_server_socket_fd != -1);
            rc = fakeSocketConnect(self.document->fakeClientFd, loolwsd_server_socket_fd);
            assert(rc != -1);

            // Create a socket pair to notify the below thread when the document has been closed
            fakeSocketPipe2(closeNotificationPipeForForwardingThread);

            // Start another thread to read responses and forward them to the JavaScript
            dispatch_async(dispatch_get_global_queue( DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
                           ^{
                               Util::setThreadName("app2js");
                               while (true) {
                                   struct pollfd p[2];
                                   p[0].fd = self.document->fakeClientFd;
                                   p[0].events = POLLIN;
                                   p[1].fd = self->closeNotificationPipeForForwardingThread[1];
                                   p[1].events = POLLIN;
                                   if (fakeSocketPoll(p, 2, -1) > 0) {
                                       if (p[1].revents == POLLIN) {
                                           // The code below handling the "BYE" fake Websocket
                                           // message has closed the other end of the
                                           // closeNotificationPipeForForwardingThread. Let's close
                                           // the other end too just for cleanliness, even if a
                                           // FakeSocket as such is not a system resource so nothing
                                           // is saved by closing it.
                                           fakeSocketClose(self->closeNotificationPipeForForwardingThread[1]);

                                           // Flag to make the inter-thread plumbing in the Online
                                           // bits go away quicker.
                                           MobileTerminationFlag = true;

                                           // Close our end of the fake socket connection to the
                                           // ClientSession thread, so that it terminates
                                           fakeSocketClose(self.document->fakeClientFd);

                                           return;
                                       }
                                       if (p[0].revents == POLLIN) {
                                           int n = fakeSocketAvailableDataLength(self.document->fakeClientFd);
                                           // I don't want to check for n being -1 here, even if
                                           // that will lead to a crash (std::length_error from the
                                           // below std::vector constructor), as n being -1 is a
                                           // sign of something being wrong elsewhere anyway, and I
                                           // prefer to fix the root cause. Let's see how well this
                                           // works out. See tdf#122543 for such a case.
                                           if (n == 0)
                                               return;
                                           std::vector<char> buf(n);
                                           n = fakeSocketRead(self.document->fakeClientFd, buf.data(), n);
                                           [self.document send2JS:buf.data() length:n];
                                       }
                                   }
                                   else
                                       break;
                               }
                               assert(false);
                           });

            // First we simply send it the URL. This corresponds to the GET request with Upgrade to
            // WebSocket.
            std::string url([[[self.document fileURL] absoluteString] UTF8String]);
            p.fd = self.document->fakeClientFd;
            p.events = POLLOUT;
            fakeSocketPoll(&p, 1, -1);
            fakeSocketWrite(self.document->fakeClientFd, url.c_str(), url.size());

            return;
        } else if ([message.body isEqualToString:@"BYE"]) {
            LOG_TRC("Document window terminating on JavaScript side. Closing our end of the socket.");

            [self bye];
            return;
        } else if ([message.body isEqualToString:@"SLIDESHOW"]) {

            // Create the SVG for the slideshow.

            self.slideshowFile = Util::createRandomTmpDir() + "/slideshow.svg";
            self.slideshowURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:self.slideshowFile.c_str()] isDirectory:NO];

            lok_document->saveAs([[self.slideshowURL absoluteString] UTF8String], "svg", nullptr);

            // Add a new full-screen WebView displaying the slideshow.

            WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];
            WKUserContentController *userContentController = [[WKUserContentController alloc] init];

            [userContentController addScriptMessageHandler:self name:@"lool"];

            configuration.userContentController = userContentController;

            self.slideshowWebView = [[WKWebView alloc] initWithFrame:CGRectZero configuration:configuration];

            [self.slideshowWebView becomeFirstResponder];

            self.slideshowWebView.contentMode = UIViewContentModeScaleAspectFit;
            self.slideshowWebView.translatesAutoresizingMaskIntoConstraints = NO;
            self.slideshowWebView.navigationDelegate = self;
            self.slideshowWebView.UIDelegate = self;

            self.webView.hidden = true;

            [self.view addSubview:self.slideshowWebView];
            [self.view bringSubviewToFront:self.slideshowWebView];


            WKWebView *slideshowWebViewP = self.slideshowWebView;
            NSDictionary *views = NSDictionaryOfVariableBindings(slideshowWebViewP);
            [self.view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"H:|-0-[slideshowWebViewP(>=0)]-0-|"
                                                                              options:0
                                                                              metrics:nil
                                                                                views:views]];
            [self.view addConstraints:[NSLayoutConstraint constraintsWithVisualFormat:@"V:|-0-[slideshowWebViewP(>=0)]-0-|"
                                                                              options:0
                                                                              metrics:nil
                                                                                views:views]];
            [self.slideshowWebView loadRequest:[NSURLRequest requestWithURL:self.slideshowURL]];

            return;
        } else if ([message.body isEqualToString:@"EXITSLIDESHOW"]) {

            std::remove(self.slideshowFile.c_str());

            [self.slideshowWebView removeFromSuperview];
            self.slideshowWebView = nil;
            self.webView.hidden = false;

            return;
        } else if ([message.body isEqualToString:@"PRINT"]) {

            // Create the PDF to print.

            std::string printFile = Util::createRandomTmpDir() + "/print.pdf";
            NSURL *printURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:printFile.c_str()] isDirectory:NO];
            lok_document->saveAs([[printURL absoluteString] UTF8String], "pdf", nullptr);

            UIPrintInteractionController *pic = [UIPrintInteractionController sharedPrintController];
            UIPrintInfo *printInfo = [UIPrintInfo printInfo];
            printInfo.outputType = UIPrintInfoOutputGeneral;
            printInfo.orientation = UIPrintInfoOrientationPortrait; // FIXME Check the document?
            printInfo.jobName = @"Document"; // FIXME

            pic.printInfo = printInfo;
            pic.printingItem = printURL;

            [pic presentFromRect:CGRectZero
                          inView:self.webView
                        animated:YES
               completionHandler:^(UIPrintInteractionController *pic, BOOL completed, NSError *error) {
                    LOG_TRC("print completion handler gets " << (completed?"YES":"NO"));
                    std::remove(printFile.c_str());
                }];

            return;
        }

        const char *buf = [message.body UTF8String];
        p.fd = self.document->fakeClientFd;
        p.events = POLLOUT;
        fakeSocketPoll(&p, 1, -1);
        fakeSocketWrite(self.document->fakeClientFd, buf, strlen(buf));
    } else {
        LOG_ERR("Unrecognized kind of message received from WebView: " << [message.name UTF8String] << ":" << [message.body UTF8String]);
    }
}

- (void)scrollViewWillBeginZooming:(UIScrollView *)scrollView withView:(UIView *)view {
    scrollView.pinchGestureRecognizer.enabled = NO;
}

- (void)bye {
    // Close one end of the socket pair, that will wake up the forwarding thread above
    fakeSocketClose(closeNotificationPipeForForwardingThread[0]);

    [self.document saveToURL:[self.document fileURL]
            forSaveOperation:UIDocumentSaveForOverwriting
           completionHandler:^(BOOL success) {
              LOG_TRC("save completion handler gets " << (success?"YES":"NO"));
           }];

    // Wait for lokit_main thread to exit
    std::lock_guard<std::mutex> lock(lokit_main_mutex);

    theSingleton = nil;

    // And only then let the document browsing view show up again
    [self dismissDocumentViewController];
}

+ (DocumentViewController*)singleton {
    return theSingleton;
}

@end

// vim:set shiftwidth=4 softtabstop=4 expandtab:
