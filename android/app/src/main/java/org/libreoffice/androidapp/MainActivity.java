/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.libreoffice.androidapp;

import android.Manifest;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.ContentResolver;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.preference.PreferenceManager;
import android.print.PrintAttributes;
import android.print.PrintDocumentAdapter;
import android.print.PrintManager;
import android.util.Log;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.webkit.JavascriptInterface;
import android.webkit.MimeTypeMap;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.widget.EditText;
import android.widget.Toast;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.net.URI;
import java.net.URISyntaxException;
import java.nio.ByteBuffer;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.nio.channels.ReadableByteChannel;
import java.util.ArrayList;
import java.util.List;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.core.content.FileProvider;
import androidx.core.util.Pair;

import org.libreoffice.androidapp.ui.FileUtilities;

public class MainActivity extends AppCompatActivity {
    final static String TAG = "MainActivity";

    private static final String ASSETS_EXTRACTED_PREFS_KEY = "ASSETS_EXTRACTED";
    private static final int PERMISSION_READ_EXTERNAL_STORAGE = 777;
    private static final String KEY_ENABLE_SHOW_DEBUG_INFO = "ENABLE_SHOW_DEBUG_INFO";

    private static final String KEY_PROVIDER_ID = "providerID";
    private static final String KEY_DOCUMENT_URI = "documentUri";
    private static final String KEY_IS_EDITABLE = "isEditable";
    private static final String KEY_INTENT_URI = "intentUri";
    private static final String KEY_DOCUMENT_EXT = "documentExtension";

    private File mTempFile = null;

    private int providerId;

    @Nullable
    private URI documentUri;

    private String documentExtension;

    private String urlToLoad;
    private WebView mWebView;
    private SharedPreferences sPrefs;
    private Handler mainHandler;

    private boolean isDocEditable = false;
    private boolean isDocDebuggable = BuildConfig.DEBUG;

    private ClipboardManager clipboardManager;
    private ClipData clipData;
    private Thread nativeMsgThread;
    private Handler nativeHandler;
    private Looper nativeLooper;

    private Pair<List<String>, List<String>> availableExportOptionsPair;
    private AlertDialog loadingAlertDialog;

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

    private void updatePreferences() {
        if (sPrefs.getInt(ASSETS_EXTRACTED_PREFS_KEY, 0) != BuildConfig.VERSION_CODE) {
            if (copyFromAssets(getAssets(), "unpack", getApplicationInfo().dataDir)) {
                sPrefs.edit().putInt(ASSETS_EXTRACTED_PREFS_KEY, BuildConfig.VERSION_CODE).apply();
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sPrefs = PreferenceManager.getDefaultSharedPreferences(getApplicationContext());
        updatePreferences();

        setContentView(R.layout.activity_main);

        AssetManager assetManager = getResources().getAssets();

        isDocDebuggable = sPrefs.getBoolean(KEY_ENABLE_SHOW_DEBUG_INFO, false) && BuildConfig.DEBUG;

        ApplicationInfo applicationInfo = getApplicationInfo();
        String dataDir = applicationInfo.dataDir;
        Log.i(TAG, String.format("Initializing LibreOfficeKit, dataDir=%s\n", dataDir));

        //redirectStdio(true);

        String cacheDir = getApplication().getCacheDir().getAbsolutePath();
        String apkFile = getApplication().getPackageResourcePath();

        if (getIntent().getData() != null) {

            if (getIntent().getData().getScheme().equals(ContentResolver.SCHEME_CONTENT)) {
                isDocEditable = false;
                Toast.makeText(this, getResources().getString(R.string.temp_file_saving_disabled), Toast.LENGTH_SHORT).show();
                if (copyFileToTemp() && mTempFile != null) {
                    documentUri = mTempFile.toURI();
                    urlToLoad = documentUri.toString();
                    documentExtension = "." + MimeTypeMap.getSingleton().getExtensionFromMimeType(getIntent().getType());
                    Log.d(TAG, "SCHEME_CONTENT: getPath(): " + getIntent().getData().getPath());
                } else {
                    // TODO: can't open the file
                    Log.e(TAG, "couldn't create temporary file from " + getIntent().getData());
                }
            } else if (getIntent().getData().getScheme().equals(ContentResolver.SCHEME_FILE)) {
                isDocEditable = true;
                urlToLoad = getIntent().getData().getPath();
                Log.d(TAG, "SCHEME_FILE: getPath(): " + getIntent().getData().getPath());
                // Gather data to rebuild IFile object later
                providerId = getIntent().getIntExtra(
                        "org.libreoffice.document_provider_id", 0);
                documentUri = (URI) getIntent().getSerializableExtra(
                        "org.libreoffice.document_uri");
                documentExtension = FileUtilities.getExtension(documentUri.toString());
            }
        } else if (savedInstanceState != null) {
            getIntent().setAction(Intent.ACTION_VIEW)
                    .setData(Uri.parse(savedInstanceState.getString(KEY_INTENT_URI)));
            urlToLoad = getIntent().getData().toString();
            providerId = savedInstanceState.getInt(KEY_PROVIDER_ID);
            documentExtension = savedInstanceState.getString(KEY_DOCUMENT_EXT);
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

        createLOOLWSD(dataDir, cacheDir, apkFile, assetManager, urlToLoad);

        mWebView = findViewById(R.id.browser);
        mWebView.setWebViewClient(new WebViewClient());

        WebSettings webSettings = mWebView.getSettings();
        webSettings.setJavaScriptEnabled(true);
        mWebView.addJavascriptInterface(this, "LOOLMessageHandler");

        // allow debugging (when building the debug version); see details in
        // https://developers.google.com/web/tools/chrome-devtools/remote-debugging/webviews
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            if ((getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0) {
                WebView.setWebContentsDebuggingEnabled(true);
            }
        }
        mainHandler = new Handler(getMainLooper());
        clipboardManager = (ClipboardManager) getSystemService(CLIPBOARD_SERVICE);
        nativeMsgThread = new Thread(() -> {
            Looper.prepare();
            nativeLooper = Looper.myLooper();
            nativeHandler = new Handler(nativeLooper);
            Looper.loop();
        });
        nativeMsgThread.start();
    }


    @Override
    protected void onStart() {
        super.onStart();
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            Log.i(TAG, "asking for read storage permission");
            ActivityCompat.requestPermissions(this,
                    new String[]{Manifest.permission.READ_EXTERNAL_STORAGE},
                    PERMISSION_READ_EXTERNAL_STORAGE);
        } else {
            loadDocument();
        }
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putString(KEY_INTENT_URI, getIntent().getData().toString());
        outState.putInt(KEY_PROVIDER_ID, providerId);
        outState.putString(KEY_DOCUMENT_EXT, documentExtension);
        if (documentUri != null) {
            outState.putString(KEY_DOCUMENT_URI, documentUri.toString());
        }
        //If this activity was opened via contentUri
        outState.putBoolean(KEY_IS_EDITABLE, isDocEditable);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        switch (requestCode) {
            case PERMISSION_READ_EXTERNAL_STORAGE:
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
    protected void onStop() {
        super.onStop();
        Log.d(TAG, "Stop LOOLWSD instance");
        nativeLooper.quit();
        postMobileMessageNative("BYE");
    }

    private void loadDocument() {
        String finalUrlToLoad = "file:///android_asset/dist/loleaflet.html?file_path=" +
                urlToLoad + "&closebutton=1";
        if (isDocEditable) {
            finalUrlToLoad += "&permission=edit";
        } else {
            finalUrlToLoad += "&permission=readonly";
        }
        if (isDocDebuggable) {
            finalUrlToLoad += "&debug=true";
        }
        mWebView.loadUrl(finalUrlToLoad);
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

        if (interceptMsgFromWebView(message)) {
            postMobileMessageNative(message);
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
    boolean interceptMsgFromWebView(String message) {
        switch (message) {
            case "PRINT":
                mainHandler.post(this::initiatePrint);
                return false;
            case "SLIDESHOW":
                initiateSlideShow();
                return false;
            case "SHARE":
                mainHandler.post(this::initiateShare);
                return false;
            case "uno .uno:Paste":
                clipData = clipboardManager.getPrimaryClip();
                if (clipData != null) {
                    if (clipData.getDescription().hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN)) {
                        ClipData.Item clipItem = clipData.getItemAt(0);
                        nativeHandler.post(() -> paste("text/plain;charset=utf-16", clipItem.getText().toString()));
                    }
                    return false;
                }
                break;
            case "uno .uno:Copy": {
                nativeHandler.post(() -> {
                    String tempSelectedText = getTextSelection();
                    if (!tempSelectedText.equals("")) {
                        clipData = ClipData.newPlainText(ClipDescription.MIMETYPE_TEXT_PLAIN, tempSelectedText);
                        clipboardManager.setPrimaryClip(clipData);
                    }
                });
                break;
            }
            case "uno .uno:Cut": {
                nativeHandler.post(() -> {
                    String tempSelectedText = getTextSelection();
                    if (!tempSelectedText.equals("")) {
                        clipData = ClipData.newPlainText(ClipDescription.MIMETYPE_TEXT_PLAIN, tempSelectedText);
                        clipboardManager.setPrimaryClip(clipData);
                    }
                });
                break;
            }
            case "SAVE_AS": {
                initiateSaveAs();
                return false;
            }
        }
        return true;
    }

    private void initiateSaveAs() {
        mainHandler.post(() -> new AlertDialog.Builder(MainActivity.this)
                .setItems(getAvailableExportOptions().second.toArray(new String[0]), (dialog, which) -> {
                    SelectPathDialogFragment selectPathDialogFragment = SelectPathDialogFragment.getInstance(getString(R.string.new_file_name,
                            "." + getAvailableExportOptions().first.get(which)));
                    selectPathDialogFragment.attachCallback(path -> {
                        Log.d(TAG, path);

                        nativeHandler.post(() -> {
                            runOnUiThread(() -> getLoadingDialog().show());
                            File tmpFile = new File(path);
                            if (!tmpFile.exists()) {
                                selectPathDialogFragment.dismiss();
                                saveAs(path, getAvailableExportOptions().first.get(which));
                                runOnUiThread(() -> Toast.makeText(MainActivity.this, getString(R.string.file_saved_at_location, path), Toast.LENGTH_SHORT).show());
                            } else {
                                runOnUiThread(() -> Toast.makeText(MainActivity.this, getString(R.string.err_file_already_exists), Toast.LENGTH_SHORT).show());
                            }
                            runOnUiThread(() -> getLoadingDialog().dismiss());
                        });

                    });
                    selectPathDialogFragment.show(getSupportFragmentManager(), "select_path_dialog");
                })
                .setTitle(getString(R.string.choose_a_format))
                .create()
                .show());

    }

    private void initiatePrint() {
        PrintManager printManager = (PrintManager) getSystemService(PRINT_SERVICE);
        PrintDocumentAdapter printAdapter = new PrintAdapter(MainActivity.this);
        printManager.print("Document", printAdapter, new PrintAttributes.Builder().build());
    }

    private void initiateSlideShow() {
        final AlertDialog slideShowProgress = new AlertDialog.Builder(this)
                .setCancelable(false)
                .setView(R.layout.dialog_loading)
                .create();

        nativeHandler.post(() -> {
            runOnUiThread(slideShowProgress::show);
            Log.v(TAG, "saving svg for slideshow by " + Thread.currentThread().getName());
            String slideShowFileUri = new File(getCacheDir(), "slideShow.svg").toURI().toString();
            saveAs(slideShowFileUri, "svg");
            runOnUiThread(() -> {
                slideShowProgress.dismiss();
                Intent slideShowActIntent = new Intent(MainActivity.this, SlideShowActivity.class);
                slideShowActIntent.putExtra(SlideShowActivity.SVG_URI_KEY, slideShowFileUri);
                startActivity(slideShowActIntent);
            });
        });
    }

    private void initiateShare() {
        mainHandler.post(() -> new AlertDialog.Builder(MainActivity.this)
                .setItems(getAvailableExportOptions().second.toArray(new String[0]), (dialog, which) -> {

                    View shareDialogFileNameView = getLayoutInflater().inflate(R.layout.dialog_share, null);
                    EditText fileNameEditTxt = shareDialogFileNameView.findViewById(R.id.nameEditTxt);
                    String defaultFileName = getString(R.string.new_file_name, "." + getAvailableExportOptions().first.get(which));
                    fileNameEditTxt.setText(defaultFileName);
                    fileNameEditTxt.setOnFocusChangeListener((v, hasFocus) -> {
                        if (hasFocus) {
                            fileNameEditTxt.setSelection(0, defaultFileName.length() - getAvailableExportOptions().first.get(which).length() - 1);
                        }
                    });

                    AlertDialog shareFileNameDialog = new AlertDialog.Builder(MainActivity.this)
                            .setTitle(getString(R.string.enter_filename))
                            .setView(shareDialogFileNameView)
                            .setPositiveButton(getString(R.string.share_dialog_positive), null)
                            .setNegativeButton(getString(R.string.share_dialog_negative), (dialog1, which1) -> dialog1.dismiss())
                            .create();

                    shareFileNameDialog.setOnShowListener(dialog1 -> {
                        fileNameEditTxt.requestFocus();
                        ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE)).toggleSoftInput(InputMethodManager.SHOW_FORCED, 0);

                        //didn't add the following in setPositiveButton because it's a workaround to stop alert dialog from dismissing when "ok" is pressed.
                        // which is required to ask the user to re-enter the filename(if it is invalid)

                        shareFileNameDialog.getButton(AlertDialog.BUTTON_POSITIVE)
                                .setOnClickListener(v -> {
                                    String fileName = fileNameEditTxt.getText().toString();

                                    if (fileName.equals("") || fileName.equals("." + getAvailableExportOptions().first.get(which))) {
                                        Toast.makeText(MainActivity.this, getString(R.string.enter_valid_filename), Toast.LENGTH_SHORT).show();
                                    } else {
                                        shareFileNameDialog.dismiss();

                                        nativeHandler.post(() -> {
                                            runOnUiThread(() -> getLoadingDialog().show());
                                            Log.v(TAG, "saving file for sharing by " + Thread.currentThread().getName());
                                            File shareFile = new File(getCacheDir(), fileName);
                                            saveAs(shareFile.toURI().toString(), getAvailableExportOptions().first.get(which));
                                            runOnUiThread(() -> getLoadingDialog().dismiss());
                                            runOnUiThread(() -> {
                                                Intent intentShareFile = new Intent(Intent.ACTION_SEND);
                                                Uri finalDocUri = FileProvider.getUriForFile(MainActivity.this,
                                                        MainActivity.this.getApplicationContext().getPackageName() + ".fileprovider",
                                                        shareFile);
                                                intentShareFile.putExtra(Intent.EXTRA_STREAM, finalDocUri);
                                                intentShareFile.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                                                intentShareFile.setDataAndType(finalDocUri, getContentResolver().getType(finalDocUri));
                                                startActivity(Intent.createChooser(intentShareFile, getString(R.string.share_document)));
                                            });
                                        });

                                    }

                                });
                    });
                    shareFileNameDialog.show();
                })
                .setTitle(getString(R.string.choose_a_format))
                .create()
                .show());
    }

    private AlertDialog getLoadingDialog() {
        if (loadingAlertDialog != null) {
            return loadingAlertDialog;
        }
        loadingAlertDialog = new AlertDialog.Builder(MainActivity.this)
                .setCancelable(false)
                .setView(R.layout.dialog_loading)
                .create();
        return loadingAlertDialog;
    }


    private Pair<List<String>, List<String>> getAvailableExportOptions() {
        if (availableExportOptionsPair != null) {
            return availableExportOptionsPair;
        }

        List<String> formats = new ArrayList<>();
        List<String> formatNames = new ArrayList<>();
        formats.add("pdf");
        formatNames.add(".pdf, Portable Document Format");

        switch (FileUtilities.getType(documentExtension)) {
            case FileUtilities.DOC: {
                formats.add("odt");
                formatNames.add(".odt, OpenDocument format");

                formats.add("txt");
                formatNames.add(".txt, Plain text");

                formats.add("rtf");
                formatNames.add(".rtf, Rich Text format");

                break;
            }

            case FileUtilities.CALC: {
                formats.add("xlsx");
                formatNames.add(".xlsx, Excel");

                formats.add("ods");
                formatNames.add(".ods, OpenDocument format");

                formats.add("csv");
                formatNames.add(".csv, Comma-separated values");

                break;
            }

            case FileUtilities.IMPRESS: {
                formats.add("pptx");
                formatNames.add(".pptx, PowerPoint");

                formats.add("odp");
                formatNames.add(".odp, OpenDocument format");

                break;
            }

            case FileUtilities.DRAWING: {
                //Todo: add formats for drawing
            }

            case FileUtilities.UNKNOWN: {
                //¯\_(ツ)_/¯
            }
        }

        availableExportOptionsPair = new Pair<>(formats, formatNames);
        return availableExportOptionsPair;
    }

    public native void saveAs(String fileUri, String format);

    public native String getTextSelection();

    public native void paste(String mimeType, String data);

}
/* vim:set shiftwidth=4 softtabstop=4 expandtab cinoptions=b1,g0,N-s cinkeys+=0=break: */
