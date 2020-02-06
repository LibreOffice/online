/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.libreoffice.androidlib;

import android.Manifest;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintManager;
import android.util.Log;
import android.view.WindowManager;
import android.webkit.JavascriptInterface;
import android.webkit.ValueCallback;
import android.webkit.WebChromeClient;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.widget.Toast;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.nio.channels.ReadableByteChannel;
import java.nio.charset.Charset;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.localbroadcastmanager.content.LocalBroadcastManager;

import org.libreoffice.androidlib.lok.LokClipboardData;
import org.libreoffice.androidlib.lok.LokClipboardEntry;

public class LOActivity extends AppCompatActivity {
    final static String TAG = "LOActivity";

    private static final String ASSETS_EXTRACTED_GIT_COMMIT = "ASSETS_EXTRACTED_GIT_COMMIT";
    private static final int PERMISSION_WRITE_EXTERNAL_STORAGE = 777;
    private static final String KEY_ENABLE_SHOW_DEBUG_INFO = "ENABLE_SHOW_DEBUG_INFO";

    private static final String KEY_PROVIDER_ID = "providerID";
    private static final String KEY_DOCUMENT_URI = "documentUri";
    private static final String KEY_IS_EDITABLE = "isEditable";
    private static final String KEY_INTENT_URI = "intentUri";
    private static final String CLIPBOARD_FILE_PATH = "LibreofficeClipboardFile.data";
    private static final String CLIPBOARD_LOOL_SIGNATURE = "lool-clip-magic-4a22437e49a8-";
    private File mTempFile = null;

    private int providerId;

    /// Unique number identifying this app + document.
    private long loadDocumentMillis = 0;

    @Nullable
    private URI documentUri;

    private String urlToLoad;
    private WebView mWebView;
    private SharedPreferences sPrefs;
    private Handler mainHandler;

    private boolean isDocEditable = false;
    private boolean isDocDebuggable = BuildConfig.DEBUG;
    private boolean documentLoaded = false;

    private ClipboardManager clipboardManager;
    private ClipData clipData;
    private Thread nativeMsgThread;
    private Handler nativeHandler;
    private Looper nativeLooper;

    /** In case the mobile-wizard is visible, we have to intercept the Android's Back button. */
    private boolean mMobileWizardVisible = false;

    private ValueCallback<Uri[]> valueCallback;
    public static final int REQUEST_SELECT_FILE = 555;

    /** Broadcasting event for passing info back to the shell. */
    public static final String LO_ACTIVITY_BROADCAST = "LOActivityBroadcast";

    /** Event description for passing info back to the shell. */
    public static final String LO_ACTION_EVENT = "LOEvent";

    /** Data description for passing info back to the shell. */
    public static final String LO_ACTION_DATA = "LOData";

    private static boolean copyFromAssets(AssetManager assetManager,
                                          String fromAssetPath, String targetDir) {
        try {
            String[] files = assetManager.list(fromAssetPath);

            boolean res = true;
            for (String file : files) {
                String[] dirOrFile = assetManager.list(fromAssetPath + "/" + file);
                if (dirOrFile.length == 0) {
                    // noinspection ResultOfMethodCallIgnored
                    new File(targetDir).mkdirs();
                    res &= copyAsset(assetManager,
                            fromAssetPath + "/" + file,
                            targetDir + "/" + file);
                } else
                    res &= copyFromAssets(assetManager,
                            fromAssetPath + "/" + file,
                            targetDir + "/" + file);
            }
            return res;
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, "copyFromAssets failed: " + e.getMessage());
            return false;
        }
    }

    private static boolean copyAsset(AssetManager assetManager, String fromAssetPath, String toPath) {
        ReadableByteChannel source = null;
        FileChannel dest = null;
        try {
            try {
                source = Channels.newChannel(assetManager.open(fromAssetPath));
                dest = new FileOutputStream(toPath).getChannel();
                long bytesTransferred = 0;
                // might not copy all at once, so make sure everything gets copied....
                ByteBuffer buffer = ByteBuffer.allocate(4096);
                while (source.read(buffer) > 0) {
                    buffer.flip();
                    bytesTransferred += dest.write(buffer);
                    buffer.clear();
                }
                Log.v(TAG, "Success copying " + fromAssetPath + " to " + toPath + " bytes: " + bytesTransferred);
                return true;
            } finally {
                if (dest != null) dest.close();
                if (source != null) source.close();
            }
        } catch (FileNotFoundException e) {
            Log.e(TAG, "file " + fromAssetPath + " not found! " + e.getMessage());
            return false;
        } catch (IOException e) {
            Log.e(TAG, "failed to copy file " + fromAssetPath + " from assets to " + toPath + " - " + e.getMessage());
            return false;
        }
    }

    /**
     * Copies fonts except the NotoSans from the system to our location.
     * This is necessary because the NotoSans is huge and fontconfig needs
     * ages to parse them.
     */
    private static boolean copyFonts(String fromPath, String targetDir) {
        try {
            File target = new File(targetDir);
            if (!target.exists())
                target.mkdirs();

            File from = new File(fromPath);
            File[] files = from.listFiles();
            for (File fontFile : files) {
                String fontFileName = fontFile.getName();
                if (!fontFileName.equals("Roboto-Regular.ttf")) {
                    Log.i(TAG, "Ignored font file: " + fontFile);
                    continue;
                } else {
                    Log.i(TAG, "Copying font file: " + fontFile);
                }

                // copy the font file over
                InputStream in = new FileInputStream(fontFile);
                try {
                    OutputStream out = new FileOutputStream(targetDir + "/" + fontFile.getName());
                    try {
                        byte[] buffer = new byte[4096];
                        int len;
                        while ((len = in.read(buffer)) > 0) {
                            out.write(buffer, 0, len);
                        }
                    } finally {
                        out.close();
                    }
                } finally {
                    in.close();
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
            Log.e(TAG, "copyFonts failed: " + e.getMessage());
            return false;
        }

        return true;
    }

    private void updatePreferences() {
        if (!sPrefs.getString(ASSETS_EXTRACTED_GIT_COMMIT, "").equals(BuildConfig.GIT_COMMIT)) {
            if (copyFromAssets(getAssets(), "unpack", getApplicationInfo().dataDir) &&
                    copyFonts("/system/fonts", getApplicationInfo().dataDir + "/user/fonts")) {
                sPrefs.edit().putString(ASSETS_EXTRACTED_GIT_COMMIT, BuildConfig.GIT_COMMIT).apply();
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        sPrefs = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
        updatePreferences();

        setContentView(R.layout.lolib_activity_main);

        isDocDebuggable = sPrefs.getBoolean(KEY_ENABLE_SHOW_DEBUG_INFO, false) && BuildConfig.DEBUG;

        if (getIntent().getData() != null) {

            if (getIntent().getData().getScheme().equals(ContentResolver.SCHEME_CONTENT)) {
                isDocEditable = false;
                Toast.makeText(this, getResources().getString(R.string.temp_file_saving_disabled), Toast.LENGTH_SHORT).show();
                if (copyFileToTemp() && mTempFile != null) {
                    documentUri = mTempFile.toURI();
                    urlToLoad = documentUri.toString();
                    Log.d(TAG, "SCHEME_CONTENT: getPath(): " + getIntent().getData().getPath());
                } else {
                    // TODO: can't open the file
                    Log.e(TAG, "couldn't create temporary file from " + getIntent().getData());
                }
            } else if (getIntent().getData().getScheme().equals(ContentResolver.SCHEME_FILE)) {
                isDocEditable = true;
                urlToLoad = getIntent().getData().toString();
                Log.d(TAG, "SCHEME_FILE: getPath(): " + getIntent().getData().getPath());
                // Gather data to rebuild IFile object later
                providerId = getIntent().getIntExtra(
                        "org.libreoffice.document_provider_id", 0);
                documentUri = (URI) getIntent().getSerializableExtra(
                        "org.libreoffice.document_uri");
            }
        } else if (savedInstanceState != null) {
            getIntent().setAction(Intent.ACTION_VIEW)
                    .setData(Uri.parse(savedInstanceState.getString(KEY_INTENT_URI)));
            urlToLoad = getIntent().getData().toString();
            providerId = savedInstanceState.getInt(KEY_PROVIDER_ID);
            if (savedInstanceState.getString(KEY_DOCUMENT_URI) != null) {
                try {
                    documentUri = new URI(savedInstanceState.getString(KEY_DOCUMENT_URI));
                    urlToLoad = documentUri.toString();
                } catch (URISyntaxException e) {
                    e.printStackTrace();
                }
            }
            isDocEditable = savedInstanceState.getBoolean(KEY_IS_EDITABLE);
        } else {
            //User can't reach here but if he/she does then
            Toast.makeText(this, getString(R.string.failed_to_load_file), Toast.LENGTH_SHORT).show();
            finish();
        }

        mWebView = findViewById(R.id.browser);

        WebSettings webSettings = mWebView.getSettings();
        webSettings.setJavaScriptEnabled(true);
        mWebView.addJavascriptInterface(this, "LOOLMessageHandler");

        boolean isChromeDebugEnabled = sPrefs.getBoolean("ENABLE_CHROME_DEBUGGING", false);
        // allow debugging (when building the debug version); see details in
        // https://developers.google.com/web/tools/chrome-devtools/remote-debugging/webviews
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0 || isChromeDebugEnabled) {
                WebView.setWebContentsDebuggingEnabled(true);
            }
        }

        mainHandler = new Handler(getMainLooper());

        clipboardManager = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        nativeMsgThread = new Thread(new Runnable() {
            @Override
            public void run() {
                Looper.prepare();
                nativeLooper = Looper.myLooper();
                nativeHandler = new Handler(nativeLooper);
                Looper.loop();
            }
        });
        nativeMsgThread.start();

        mWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public boolean onShowFileChooser(WebView mWebView, ValueCallback<Uri[]> filePathCallback, WebChromeClient.FileChooserParams fileChooserParams) {
                if (valueCallback != null) {
                    valueCallback.onReceiveValue(null);
                    valueCallback = null;
                }

                valueCallback = filePathCallback;
                Intent intent = fileChooserParams.createIntent();

                try {
                    intent.setType("image/*");
                    startActivityForResult(intent, REQUEST_SELECT_FILE);
                } catch (ActivityNotFoundException e) {
                    valueCallback = null;
                    Toast.makeText(LOActivity.this, getString(R.string.cannot_open_file_chooser), Toast.LENGTH_LONG).show();
                    return false;
                }
                return true;
            }
        });

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            Log.i(TAG, "asking for read storage permission");
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE},
                    PERMISSION_WRITE_EXTERNAL_STORAGE);
        } else {
            loadDocument();
        }
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(KEY_INTENT_URI, getIntent().getData().toString());
        outState.putInt(KEY_PROVIDER_ID, providerId);
        if (documentUri != null) {
            outState.putString(KEY_DOCUMENT_URI, documentUri.toString());
        }
        //If this activity was opened via contentUri
        outState.putBoolean(KEY_IS_EDITABLE, isDocEditable);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case PERMISSION_WRITE_EXTERNAL_STORAGE:
                if (permissions.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                    loadDocument();
                } else {
                    Toast.makeText(this, getString(R.string.storage_permission_required), Toast.LENGTH_SHORT).show();
                    finish();
                    break;
                }
                break;
            default:
                super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }

    private boolean copyFileToTemp() {
        ContentResolver contentResolver = getContentResolver();
        FileChannel inputChannel = null;
        FileChannel outputChannel = null;
        // CSV files need a .csv suffix to be opened in Calc.
        String suffix = null;
        String intentType = getIntent().getType();
        // K-9 mail uses the first, GMail uses the second variant.
        if ("text/comma-separated-values".equals(intentType) || "text/csv".equals(intentType))
            suffix = ".csv";

        try {
            try {
                AssetFileDescriptor assetFD = contentResolver.openAssetFileDescriptor(getIntent().getData(), "r");
                if (assetFD == null) {
                    Log.e(TAG, "couldn't create assetfiledescriptor from " + getIntent().getDataString());
                    return false;
                }
                inputChannel = assetFD.createInputStream().getChannel();
                mTempFile = File.createTempFile("LibreOffice", suffix, this.getCacheDir());

                outputChannel = new FileOutputStream(mTempFile).getChannel();
                long bytesTransferred = 0;
                // might not  copy all at once, so make sure everything gets copied....
                while (bytesTransferred < inputChannel.size()) {
                    bytesTransferred += outputChannel.transferFrom(inputChannel, bytesTransferred, inputChannel.size());
                }
                Log.e(TAG, "Success copying " + bytesTransferred + " bytes");
                return true;
            } finally {
                if (inputChannel != null) inputChannel.close();
                if (outputChannel != null) outputChannel.close();
            }
        } catch (FileNotFoundException e) {
            return false;
        } catch (IOException e) {
            return false;
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i(TAG, "onResume..");

        // check for config change
        updatePreferences();
    }

    @Override
    protected void onPause() {
        super.onPause();
        Log.d(TAG, "onPause() - hinting to save, we might need to return to the doc");

        // A Save similar to an autosave
        if (documentLoaded)
            postMobileMessageNative("save dontTerminateEdit=true dontSaveIfUnmodified=true");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "onDestroy() - we know we are leaving the document");
        nativeLooper.quit();
        mWebView.destroy();
        postMobileMessageNative("BYE");
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) {
        if (requestCode == REQUEST_SELECT_FILE) {
            if (valueCallback == null)
                return;
            valueCallback.onReceiveValue(WebChromeClient.FileChooserParams.parseResult(resultCode, intent));
            valueCallback = null;
        } else {
            Toast.makeText(this, getString(R.string.failed_to_insert_image), Toast.LENGTH_LONG).show();
        }
    }

    @Override
    public void onBackPressed() {
        if (mMobileWizardVisible)
        {
            // just return one level up in the mobile-wizard (or close it)
            callFakeWebsocketOnMessage("'mobile: mobilewizardback'");
            return;
        }

        super.onBackPressed();
    }

    private void loadDocument() {
        // setup the LOOLWSD
        ApplicationInfo applicationInfo = getApplicationInfo();
        String dataDir = applicationInfo.dataDir;
        Log.i(TAG, String.format("Initializing LibreOfficeKit, dataDir=%s\n", dataDir));

        String cacheDir = getApplication().getCacheDir().getAbsolutePath();
        String apkFile = getApplication().getPackageResourcePath();
        AssetManager assetManager = getResources().getAssets();

        createLOOLWSD(dataDir, cacheDir, apkFile, assetManager, urlToLoad);

        // trigger the load of the document
        String finalUrlToLoad = "file:///android_asset/dist/loleaflet.html?file_path=" +
                urlToLoad + "&closebutton=1";

        // set the language
        finalUrlToLoad += "&lang=" + getResources().getConfiguration().locale.toLanguageTag();

        if (isDocEditable) {
            finalUrlToLoad += "&permission=edit";
        } else {
            finalUrlToLoad += "&permission=readonly";
        }

        if (isDocDebuggable) {
            finalUrlToLoad += "&debug=true";
        }

        // load the page
        mWebView.loadUrl(finalUrlToLoad);

        documentLoaded = true;

        loadDocumentMillis = android.os.SystemClock.uptimeMillis();
    }

    static {
        System.loadLibrary("androidapp");
    }

    /**
     * Initialize the LOOLWSD to load 'loadFileURL'.
     */
    public native void createLOOLWSD(String dataDir, String cacheDir, String apkFile, AssetManager assetManager, String loadFileURL);

    /**
     * Passing messages from JS (instead of the websocket communication).
     */
    @JavascriptInterface
    public void postMobileMessage(String message) {
        Log.d(TAG, "postMobileMessage: " + message);

        String[] messageAndParameterArray= message.split(" ", 2); // the command and the rest (that can potentially contain spaces too)

        if (beforeMessageFromWebView(messageAndParameterArray)) {
            postMobileMessageNative(message);
            afterMessageFromWebView(messageAndParameterArray);
        }

        // Going back to document browser on BYE (called when pressing the top left exit button)
        if (message.equals("BYE"))
            finish();
    }

    /**
     * Call the post method form C++
     */
    public native void postMobileMessageNative(String message);

    /**
     * Passing messages from JS (instead of the websocket communication).
     */
    @JavascriptInterface
    public void postMobileError(String message) {
        // TODO handle this
        Log.d(TAG, "postMobileError: " + message);
    }

    /**
     * Passing messages from JS (instead of the websocket communication).
     */
    @JavascriptInterface
    public void postMobileDebug(String message) {
        // TODO handle this
        Log.d(TAG, "postMobileDebug: " + message);
    }

    /**
     * Passing message the other way around - from Java to the FakeWebSocket in JS.
     */
    void callFakeWebsocketOnMessage(final String message) {
        // call from the UI thread
        mWebView.post(new Runnable() {
            public void run() {
                Log.i(TAG, "Forwarding to the WebView: " + message);
                mWebView.loadUrl("javascript:window.TheFakeWebSocket.onmessage({'data':" + message + "});");
            }
        });
    }

    /**
     * return true to pass the message to the native part or false to block the message
     */
    private boolean beforeMessageFromWebView(String[] messageAndParam) {
        switch (messageAndParam[0]) {
            case "PRINT":
                mainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        LOActivity.this.initiatePrint();
                    }
                });
                return false;
            case "SLIDESHOW":
                initiateSlideShow();
                return false;
            case "SAVE":
                sendBroadcast(messageAndParam[0], messageAndParam[1]);
                return false;
            case "uno":
                switch (messageAndParam[1]) {
                    case ".uno:Paste":
                        return performPaste();
                    default:
                        break;
                }
                break;
            case "DIM_SCREEN": {
                mainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    }
                });
                return false;
            }
            case "LIGHT_SCREEN": {
                mainHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
                    }
                });
                return false;
            }
            case "MOBILEWIZARD": {
                switch (messageAndParam[1]) {
                    case "show":
                        mMobileWizardVisible = true;
                        break;
                    case "hide":
                        mMobileWizardVisible = false;
                        break;
                }
                return false;
            }
        }
        return true;
    }

    private void afterMessageFromWebView(String[] messageAndParameterArray) {
        switch (messageAndParameterArray[0]) {
            case "uno":
                switch (messageAndParameterArray[1]) {
                    case ".uno:Copy":
                    case ".uno:Cut":
                        populateClipboard();
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }

    private void initiatePrint() {
        PrintManager printManager = (PrintManager) getSystemService(PRINT_SERVICE);
        PrintDocumentAdapter printAdapter = new PrintAdapter(LOActivity.this);
        printManager.print("Document", printAdapter, new PrintAttributes.Builder().build());
    }

    private void initiateSlideShow() {
        final AlertDialog slideShowProgress = new AlertDialog.Builder(this)
                .setCancelable(false)
                .setView(R.layout.lolib_dialog_loading)
                .create();

        slideShowProgress.show();

        nativeHandler.post(new Runnable() {
            @Override
            public void run() {
                Log.v(TAG, "saving svg for slideshow by " + Thread.currentThread().getName());
                final String slideShowFileUri = new File(LOActivity.this.getCacheDir(), "slideShow.svg").toURI().toString();
                LOActivity.this.saveAs(slideShowFileUri, "svg");
                LOActivity.this.runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        slideShowProgress.dismiss();
                        Intent slideShowActIntent = new Intent(LOActivity.this, SlideShowActivity.class);
                        slideShowActIntent.putExtra(SlideShowActivity.SVG_URI_KEY, slideShowFileUri);
                        LOActivity.this.startActivity(slideShowActIntent);
                    }
                });
            }
        });
    }

    /** Send message back to the shell (for example for the cloud save). */
    public void sendBroadcast(String event, String data) {
        Intent intent = new Intent(LO_ACTIVITY_BROADCAST);
        intent.putExtra(LO_ACTION_EVENT, event);
        intent.putExtra(LO_ACTION_DATA, data);
        LocalBroadcastManager.getInstance(this).sendBroadcast(intent);
    }

    public native void saveAs(String fileUri, String format);

    public native boolean getClipboardContent(LokClipboardData aData);

    public native void setClipboardContent(LokClipboardData aData);

    public native void paste(String mimeType, byte[] data);

    public native void postUnoCommand(String command, String arguments, boolean bNotifyWhenFinished);

    /// Returns a magic that specifies this application - and this document.
    private final String getClipboardMagic() {
        return CLIPBOARD_LOOL_SIGNATURE + Long.toString(loadDocumentMillis);
    }

    /// Needs to be executed after the .uno:Copy / Paste has executed
    public final void populateClipboard()
    {
        File clipboardFile = new File(getApplicationContext().getCacheDir(), CLIPBOARD_FILE_PATH);
        if (clipboardFile.exists())
            clipboardFile.delete();

        LokClipboardData clipboardData = new LokClipboardData();
        if (!LOActivity.this.getClipboardContent(clipboardData))
            Log.e(TAG, "no clipboard to copy");
        else
        {
            clipboardData.writeToFile(clipboardFile);

            String text = clipboardData.getText();
            String html = clipboardData.getHtml();

            if (html != null) {
                int idx = html.indexOf("<meta name=\"generator\" content=\"");

                if (idx < 0)
                    idx = html.indexOf("<meta http-equiv=\"content-type\" content=\"text/html;");

                if (idx >= 0) { // inject our magic
                    StringBuffer newHtml = new StringBuffer(html);
                    newHtml.insert(idx, "<meta name=\"origin\" content=\"" + getClipboardMagic() + "\"/>\n");
                    html = newHtml.toString();
                }

                if (text == null || text.length() == 0)
                    Log.i(TAG, "set text to clipoard with: text '" + text + "' and html '" + html + "'");

                clipData = ClipData.newHtmlText(ClipDescription.MIMETYPE_TEXT_HTML, text, html);
                clipboardManager.setPrimaryClip(clipData);
            }
        }
    }

    /// Do the paste, and return true if we should short-circuit the paste locally
    private final boolean performPaste()
    {
        clipData = clipboardManager.getPrimaryClip();
        ClipDescription clipDesc = clipData != null ? clipData.getDescription() : null;
        if (clipDesc != null) {
            if (clipDesc.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) {
                final String html = clipData.getItemAt(0).getHtmlText();
                // Check if the clipboard content was made with the app
                if (html.contains(CLIPBOARD_LOOL_SIGNATURE)) {
                    // Check if the clipboard content is from the same app instance
                    if (html.contains(getClipboardMagic())) {
                        Log.i(TAG, "clipboard comes from us - same instance: short circuit it " + html);
                        return true;
                    } else {
                        Log.i(TAG, "clipboard comes from us - other instance: paste from clipboard file");

                        File clipboardFile = new File(getApplicationContext().getCacheDir(), CLIPBOARD_FILE_PATH);
                        LokClipboardData clipboardData = null;
                        if (clipboardFile.exists()) {
                            clipboardData = new LokClipboardData();
                            clipboardData.loadFromFile(clipboardFile);
                        }
                        if (clipboardData != null) {
                            LOActivity.this.setClipboardContent(clipboardData);
                            LOActivity.this.postUnoCommand(".uno:Paste", null, false);
                        } else {
                            // Couldn't get data from the clipboard file, but we can still paste html
                            byte[] htmlByteArray = html.getBytes(Charset.forName("UTF-8"));
                            LOActivity.this.paste("text/html", htmlByteArray);
                        }
                    }
                } else {
                    Log.i(TAG, "foreign html '" + html + "'");
                    byte[] htmlByteArray = html.getBytes(Charset.forName("UTF-8"));
                    LOActivity.this.paste("text/html", htmlByteArray);
                }
            }
            else if (clipDesc.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) {
                final ClipData.Item clipItem = clipData.getItemAt(0);
                String text = clipItem.getText().toString();
                byte[] textByteArray = text.getBytes(Charset.forName("UTF-16"));
                LOActivity.this.paste("text/plain;charset=utf-16", textByteArray);
            }
        }
        return false;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
