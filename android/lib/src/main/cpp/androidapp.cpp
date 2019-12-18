/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config.h>
#include <jni.h>
#include <android/log.h>

#include <chrono>
#include <thread>

#include <FakeSocket.hpp>
#include <Kit.hpp>
#include <Log.hpp>
#include <LOOLWSD.hpp>
#include <Protocol.hpp>
#include <Util.hpp>

#include <osl/detail/android-bootstrap.h>

#include <Poco/Base64Encoder.h>

const int SHOW_JS_MAXLEN = 70;

int loolwsd_server_socket_fd = -1;

static std::string fileURL;
static int fakeClientFd;
static int closeNotificationPipeForForwardingThread[2] = {-1, -1};
static JavaVM* javaVM = nullptr;
static bool lokInitialized = false;
static std::mutex loolwsdRunningMutex;

extern "C" JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM* vm, void*) {
    javaVM = vm;
    libreofficekit_set_javavm(vm);

    JNIEnv* env;
    if (vm->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR; // JNI version not supported.
    }

    setenv("SAL_LOK_OPTIONS", "unipoll", 0);

    // Uncomment the following to see the logs from the core too
    //setenv("SAL_LOG", "+WARN+INFO", 0);

    Log::initialize("Mobile", "debug", false, false, {});

    return JNI_VERSION_1_6;
}

static void send2JS(jclass loActivityClz, jobject loActivityObj, const std::vector<char>& buffer)
{
    LOG_DBG("Send to JS: " << LOOLProtocol::getAbbreviatedMessage(buffer.data(), buffer.size()));

    std::string js;

    // Check if the message is binary. We say that any message that isn't just a single line is
    // "binary" even if that strictly speaking isn't the case; for instance the commandvalues:
    // message has a long bunch of non-binary JSON on multiple lines. But _onMessage() in Socket.js
    // handles it fine even if such a message, too, comes in as an ArrayBuffer. (Look for the
    // "textMsg = String.fromCharCode.apply(null, imgBytes);".)

    const char *newline = (const char *)memchr(buffer.data(), '\n', buffer.size());
    if (newline != nullptr)
    {
        // The data needs to be an ArrayBuffer
        std::stringstream ss;
        ss << "Base64ToArrayBuffer('";

        Poco::Base64Encoder encoder(ss);
        encoder.rdbuf()->setLineLength(0); // unlimited
        encoder << std::string(buffer.data(), buffer.size());
        encoder.close();

        ss << "')";

        js = ss.str();
    }
    else
    {
        const unsigned char *ubufp = (const unsigned char *)buffer.data();
        std::vector<char> data;
        data.push_back('\'');
        for (int i = 0; i < buffer.size(); i++)
        {
            if (ubufp[i] < ' ' || ubufp[i] == '\'' || ubufp[i] == '\\')
            {
                data.push_back('\\');
                data.push_back('x');
                data.push_back("0123456789abcdef"[(ubufp[i] >> 4) & 0x0F]);
                data.push_back("0123456789abcdef"[ubufp[i] & 0x0F]);
            }
            else
            {
                data.push_back(ubufp[i]);
            }
        }
        data.push_back('\'');

        js = std::string(data.data(), data.size());
    }

    std::string subjs = js.substr(0, std::min(std::string::size_type(SHOW_JS_MAXLEN), js.length()));
    if (js.length() > SHOW_JS_MAXLEN)
        subjs += "...";

    LOG_DBG("Sending to JavaScript: " << subjs);

    JNIEnv *env;
    jint res = javaVM->GetEnv((void**)&env, JNI_VERSION_1_6);
    if (res == JNI_EDETACHED) {
        LOG_DBG("GetEnv need to attach thread");
        res = javaVM->AttachCurrentThread(&env, nullptr);
        if (JNI_OK != res) {
            LOG_DBG("Failed to AttachCurrentThread");
            return;
        }
    }
    else if (res == JNI_EVERSION) {
        LOG_DBG("GetEnv version not supported");
        return;
    }
    else if (res != JNI_OK) {
        LOG_DBG("GetEnv another error");
        return;
    }

    jstring jstr = env->NewStringUTF(js.c_str());
    jmethodID callFakeWebsocket = env->GetMethodID(loActivityClz, "callFakeWebsocketOnMessage", "(Ljava/lang/String;)V");
    env->CallVoidMethod(loActivityObj, callFakeWebsocket, jstr);

    if (env->ExceptionCheck())
        env->ExceptionDescribe();

    javaVM->DetachCurrentThread();
}

/// Close the document.
void closeDocument()
{
    // Close one end of the socket pair, that will wake up the forwarding thread that was constructed in HULLO
    if (fakeSocketClose(closeNotificationPipeForForwardingThread[0]) == 0)
    {
        LOG_DBG("Waiting for LOOLWSD to finish...");
        std::unique_lock<std::mutex> lock(loolwsdRunningMutex);
        LOG_DBG("LOOLWSD has finished.");
    }
}

/// Handle a message from JavaScript.
extern "C" JNIEXPORT void JNICALL
Java_org_libreoffice_androidlib_LOActivity_postMobileMessageNative(JNIEnv *env, jobject instance, jstring message)
{
    const char *string_value = env->GetStringUTFChars(message, nullptr);

    if (string_value)
    {
        LOG_DBG("From JS: lool: " << string_value);

        // we need a copy, because we can get a new one while we are still
        // taking down the old one
        const int currentFakeClientFd = fakeClientFd;

        if (strcmp(string_value, "HULLO") == 0)
        {
            // Now we know that the JS has started completely

            // Contact the permanently (during app lifetime) listening LOOLWSD server
            // "public" socket
            assert(loolwsd_server_socket_fd != -1);

            int rc = fakeSocketConnect(currentFakeClientFd, loolwsd_server_socket_fd);
            assert(rc != -1);

            // Create a socket pair to notify the below thread when the document has been closed
            fakeSocketPipe2(closeNotificationPipeForForwardingThread);

            // Start another thread to read responses and forward them to the JavaScript
            jclass clz = env->GetObjectClass(instance);
            jclass loActivityClz = (jclass) env->NewGlobalRef(clz);
            jobject loActivityObj = env->NewGlobalRef(instance);

            std::thread([loActivityClz, loActivityObj, currentFakeClientFd]
                        {
                            Util::setThreadName("app2js");
                            while (true)
                            {
                               struct pollfd pollfd[2];
                               pollfd[0].fd = currentFakeClientFd;
                               pollfd[0].events = POLLIN;
                               pollfd[1].fd = closeNotificationPipeForForwardingThread[1];
                               pollfd[1].events = POLLIN;
                               if (fakeSocketPoll(pollfd, 2, -1) > 0)
                               {
                                   if (pollfd[1].revents == POLLIN)
                                   {
                                       LOG_DBG("app2js: closing the sockets");
                                       // The code below handling the "BYE" fake Websocket
                                       // message has closed the other end of the
                                       // closeNotificationPipeForForwardingThread. Let's close
                                       // the other end too just for cleanliness, even if a
                                       // FakeSocket as such is not a system resource so nothing
                                       // is saved by closing it.
                                       fakeSocketClose(closeNotificationPipeForForwardingThread[1]);

                                       // Flag to make the inter-thread plumbing in the Online
                                       // bits go away quicker.
                                       MobileTerminationFlag = true;

                                       // Close our end of the fake socket connection to the
                                       // ClientSession thread, so that it terminates
                                       fakeSocketClose(currentFakeClientFd);

                                       return;
                                   }
                                   if (pollfd[0].revents == POLLIN)
                                   {
                                       int n = fakeSocketAvailableDataLength(currentFakeClientFd);
                                       if (n == 0)
                                           return;
                                       std::vector<char> buf(n);
                                       n = fakeSocketRead(currentFakeClientFd, buf.data(), n);
                                       send2JS(loActivityClz, loActivityObj, buf);
                                   }
                               }
                               else
                                   break;
                           }
                           assert(false);
                        }).detach();

            // First we simply send it the URL. This corresponds to the GET request with Upgrade to
            // WebSocket.
            LOG_DBG("Actually sending to Online:" << fileURL);

            // Send the document URL to LOOLWSD to setup the docBroker URL
            struct pollfd pollfd;
            pollfd.fd = currentFakeClientFd;
            pollfd.events = POLLOUT;
            fakeSocketPoll(&pollfd, 1, -1);
            fakeSocketWrite(currentFakeClientFd, fileURL.c_str(), fileURL.size());
        }
        else if (strcmp(string_value, "BYE") == 0)
        {
            LOG_DBG("Document window terminating on JavaScript side. Closing our end of the socket.");

            closeDocument();
        }
        else
        {
            // Send the message to LOOLWSD
            char *string_copy = strdup(string_value);

            struct pollfd pollfd;
            pollfd.fd = currentFakeClientFd;
            pollfd.events = POLLOUT;
            fakeSocketPoll(&pollfd, 1, -1);
            fakeSocketWrite(currentFakeClientFd, string_copy, strlen(string_copy));

            free(string_copy);
        }
    }
    else
        LOG_DBG("From JS: lool: some object");
}

extern "C" jboolean libreofficekit_initialize(JNIEnv* env, jstring dataDir, jstring cacheDir, jstring apkFile, jobject assetManager);

/// Create the LOOLWSD instance.
extern "C" JNIEXPORT void JNICALL
Java_org_libreoffice_androidlib_LOActivity_createLOOLWSD(JNIEnv *env, jobject, jstring dataDir, jstring cacheDir, jstring apkFile, jobject assetManager, jstring loadFileURL)
{
    fileURL = std::string(env->GetStringUTFChars(loadFileURL, nullptr));

    // already initialized?
    if (lokInitialized)
    {
        // close the previous document so that we can wait for the new HULLO
        closeDocument();
        return;
    }

    lokInitialized = true;
    libreofficekit_initialize(env, dataDir, cacheDir, apkFile, assetManager);

    Util::setThreadName("main");

    fakeSocketSetLoggingCallback([](const std::string& line)
                                 {
                                     LOG_DBG(line);
                                 });

    std::thread([]
                {
                    char *argv[2];
                    argv[0] = strdup("mobile");
                    argv[1] = nullptr;
                    Util::setThreadName("app");
                    while (true)
                    {
                        LOG_DBG("Creating LOOLWSD");
                        {
                            std::unique_lock<std::mutex> lock(loolwsdRunningMutex);
                            fakeClientFd = fakeSocketSocket();
                            LOG_DBG("createLOOLWSD created fakeClientFd: " << fakeClientFd);
                            std::unique_ptr<LOOLWSD> loolwsd(new LOOLWSD());
                            loolwsd->run(1, argv);
                        }
                        LOG_DBG("One run of LOOLWSD completed");
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }).detach();
}

extern "C"
JNIEXPORT void JNICALL
Java_org_libreoffice_androidlib_LOActivity_saveAs(JNIEnv *env, jobject instance,
                                                    jstring fileUri_, jstring format_) {
    const char *fileUri = env->GetStringUTFChars(fileUri_, 0);
    const char *format = env->GetStringUTFChars(format_, 0);

    getLOKDocument()->saveAs(fileUri, format, nullptr);

    env->ReleaseStringUTFChars(fileUri_, fileUri);
    env->ReleaseStringUTFChars(format_, format);
}

static jstring tojstringAndFree(JNIEnv *env, char *str)
{
    if (!str)
        return env->NewStringUTF("");
    jstring ret = env->NewStringUTF(str);
    free(str);
    return ret;
}

extern "C"
JNIEXPORT jobjectArray JNICALL
Java_org_libreoffice_androidlib_LOActivity_getClipboardContent(JNIEnv *env, jobject instance)
{
    const char *mimeTypes[] = { "text/plain;charset=utf-8", "text/html", nullptr };
    size_t outCount = 0;
    char  **outMimeTypes = nullptr;
    size_t *outSizes = nullptr;
    char  **outStreams = nullptr;

    jobjectArray values = (jobjectArray)env->NewObjectArray(2,env->FindClass("java/lang/String"),env->NewStringUTF(""));

    if (getLOKDocument()->getClipboard(mimeTypes,
                                       &outCount, &outMimeTypes,
                                       &outSizes, &outStreams))
    {
        if (outCount != 2)
            LOG_DBG("clipboard fetch produced wrong results");
        else
        {
            env->SetObjectArrayElement(values,0,tojstringAndFree(env, outStreams[0]));
            env->SetObjectArrayElement(values,1,tojstringAndFree(env, outStreams[1]));
            free (outMimeTypes[0]);
            free (outMimeTypes[1]);
            free (outMimeTypes);
            free (outStreams);
        }
    }
    else
        LOG_DBG("failed to fetch mime-types");

    return values;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_libreoffice_androidlib_LOActivity_paste(JNIEnv *env, jobject instance, jstring mimeType_,
                                                   jstring data_) {
    const char *mimeType = env->GetStringUTFChars(mimeType_, 0);
    const char *data = env->GetStringUTFChars(data_, 0);
    const size_t nSize = env->GetStringLength(data_);

    getLOKDocument()->paste(mimeType, data, nSize);

    env->ReleaseStringUTFChars(mimeType_, mimeType);
    env->ReleaseStringUTFChars(data_, data);
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
