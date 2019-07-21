/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.libreoffice.androidapp;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.os.AsyncTask;
import android.os.Bundle;
import android.util.Log;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;

import org.libreoffice.androidapp.storage.DocumentProviderFactory;
import org.libreoffice.androidapp.storage.IDocumentProvider;
import org.libreoffice.androidapp.storage.IFile;

import java.io.File;
import java.util.ArrayList;
import java.util.List;

public class SelectPathDialogFragment extends DialogFragment {
    private String TAG = getClass().getSimpleName();
    private static String PREFILLED_FILENAME = "preFilledFileName";

    private List<IFile> filePaths = new ArrayList<>();
    private DocumentProviderFactory documentProviderFactory = DocumentProviderFactory.getInstance();
    private IDocumentProvider documentProvider;
    private IFile homeDirectory;
    private IFile currentDirectory;
    private List<Pair<String, Drawable>> storageOptions = new ArrayList<>();

    private ListView directoryList;
    private TextView directoryPath;
    private LinearLayout inputManagerLayout;

    private OnCompleteListener onCompleteListener;

    public static SelectPathDialogFragment getInstance(String preFilledFileName) {
        SelectPathDialogFragment selectPathDialogFragment = new SelectPathDialogFragment();
        Bundle args = new Bundle();
        args.putString(PREFILLED_FILENAME, preFilledFileName);
        selectPathDialogFragment.setArguments(args);
        return selectPathDialogFragment;
    }

    @SuppressWarnings("unchecked")
    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        View v = inflater.inflate(R.layout.dialog_select_path, container);
        directoryList = v.findViewById(R.id.directory_list);
        directoryPath = v.findViewById(R.id.directory_path);
        inputManagerLayout = v.findViewById(R.id.manage_input_linear_layout);
        TextView fileName = v.findViewById(R.id.file_name);
        if (getArguments() != null) {
            fileName.setText(getArguments().getString(PREFILLED_FILENAME));
        }
        v.findViewById(R.id.done).setOnClickListener(v1 -> {
            if (fileName.getText().toString().equals("")) {
                Toast.makeText(getContext(), getString(R.string.enter_filename), Toast.LENGTH_SHORT).show();
            } else {
                onCompleteListener.onComplete(currentDirectory.getUri().getPath() + fileName.getText().toString());
            }
        });
        storageOptions.clear();
        storageOptions.add(new Pair(getString(R.string.local_file_system), getContext().getDrawable(R.drawable.ic_storage_black_24dp)));
        storageOptions.add(new Pair(getString(R.string.external_sd_file_system), getContext().getDrawable(R.drawable.ic_sd_card_black_24dp)));
        storageOptions.add(new Pair(getString(R.string.otg_file_system), getContext().getDrawable(R.drawable.ic_usb_black_24dp)));
        directoryList.setAdapter(new DirectoryListAdapter(getContext(), R.layout.file_list_item, filePaths));
        return v;
    }

    private void switchToDocumentProvider(IDocumentProvider provider) {

        new AsyncTask<IDocumentProvider, Void, Void>() {
            @Override
            protected Void doInBackground(IDocumentProvider... provider) {
                try {
                    homeDirectory = provider[0].getRootDirectory(getActivity());
                    List<IFile> paths = homeDirectory.listFiles();
                    filePaths = new ArrayList<>();
                    for (IFile file : paths) {
                        if (file.isDirectory()) {
                            filePaths.add(file);
                        }
                    }
                } catch (final RuntimeException e) {
                    getActivity().runOnUiThread(() -> {
                        Toast.makeText(getActivity(), e.getMessage(),
                                Toast.LENGTH_SHORT).show();
                        SelectPathDialogFragment.this.dismiss();
                    });
                    Log.e(TAG, "failed to switch document provider " + e.getMessage(), e.getCause());
                    return null;
                }
                //no exception
                documentProvider = provider[0];
                currentDirectory = homeDirectory;
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                ((DirectoryListAdapter) directoryList.getAdapter()).setFilePaths(filePaths);
            }
        }.execute(provider);
    }

    private void openDirectory(IFile dir) {
        if (dir == null)
            return;

        new AsyncTask<IFile, Void, Void>() {
            @Override
            protected Void doInBackground(IFile... dir) {
                currentDirectory = dir[0];
                try {
                    List<IFile> paths = currentDirectory.listFiles();
                    filePaths = new ArrayList<>();
                    for (IFile file : paths) {
                        if (file.isDirectory()) {
                            filePaths.add(file);
                        }
                    }
                } catch (final RuntimeException e) {
                    getActivity().runOnUiThread(() -> {
                        Toast.makeText(getActivity(), e.getMessage(),
                                Toast.LENGTH_SHORT).show();
                        SelectPathDialogFragment.this.dismiss();
                    });
                    Log.e(TAG, "failed to switch document provider " + e.getMessage(), e.getCause());
                    return null;
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void result) {
                ((DirectoryListAdapter) directoryList.getAdapter()).setFilePaths(filePaths);
            }
        }.execute(dir);
    }

    private void openParentDirectory() {
        if (currentDirectory.equals(homeDirectory)) {
            homeDirectory = null;
            ((DirectoryListAdapter) directoryList.getAdapter()).notifyDataSetChanged();
            return;
        }
        new AsyncTask<Void, Void, IFile>() {
            @Override
            protected IFile doInBackground(Void... dir) {
                return currentDirectory.getParent(getActivity());
            }

            @Override
            protected void onPostExecute(IFile result) {
                openDirectory(result);
            }
        }.execute();
    }

    private class DirectoryListAdapter extends ArrayAdapter<IFile> {

        private List<IFile> filePaths;
        private Context context;
        private int resource;

        DirectoryListAdapter(@NonNull Context context, int resource, List<IFile> objects) {
            super(context, resource);
            this.filePaths = objects;
            this.context = context;
            this.resource = resource;
        }

        void setFilePaths(List<IFile> objects) {
            filePaths.clear();
            //Up Directory shortcut
            filePaths.add(null);
            filePaths.addAll(objects);
            super.notifyDataSetChanged();
        }

        @NonNull
        @Override
        public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
            if (convertView == null) {
                convertView = LayoutInflater.from(context).inflate(resource, parent, false);
            }

            convertView.findViewById(R.id.file_item_size).setVisibility(View.GONE);
            convertView.findViewById(R.id.file_item_date).setVisibility(View.GONE);
            TextView fileItemName = convertView.findViewById(R.id.file_item_name);
            ImageView fileItemIcon = convertView.findViewById(R.id.file_item_icon);
            fileItemName.setTextColor(Color.BLACK);
            fileItemIcon.setColorFilter(Color.BLACK);
            inputManagerLayout.setVisibility(View.VISIBLE);

            if (homeDirectory == null) {
                directoryPath.setVisibility(View.INVISIBLE);
                inputManagerLayout.setVisibility(View.GONE);
                fileItemName.setText(storageOptions.get(position).first);
                fileItemIcon.setImageDrawable(storageOptions.get(position).second);
                if (!documentProviderFactory.getProvider(position + 1).checkProviderAvailability(context)) {
                    fileItemName.setTextColor(Color.GRAY);
                    fileItemIcon.setColorFilter(Color.GRAY);
                    convertView.setOnClickListener(v -> {
                        Toast.makeText(context, getString(R.string.err_storage_option_not_available), Toast.LENGTH_SHORT).show();
                    });
                } else {
                    convertView.setOnClickListener(v -> {
                        //(position+1) is workaround to save some if/else;
                        switchToDocumentProvider(documentProviderFactory.getProvider(position + 1));
                    });
                }
            } else {
                directoryPath.setVisibility(View.VISIBLE);
                directoryPath.setText(currentDirectory.getUri().getPath());
                fileItemIcon.setImageDrawable(context.getDrawable(R.drawable.ic_folder_black_24dp));
                //Up directory at position == 0;
                if (position == 0) {
                    fileItemName.setText("..");
                    convertView.setOnClickListener(v -> {
                        if (currentDirectory == homeDirectory) {
                            homeDirectory = null;
                            notifyDataSetChanged();
                        } else {
                            openParentDirectory();
                        }
                    });
                } else {
                    fileItemName.setText(filePaths.get(position).getName());
                    convertView.setOnClickListener(v -> openDirectory(filePaths.get(position)));
                }
            }

            return convertView;
        }

        @Override
        public int getCount() {
            if (homeDirectory == null) {
                return storageOptions.size();
            } else {
                return filePaths.size();
            }
        }
    }

    void attachCallback(OnCompleteListener onCompleteListener) {
        this.onCompleteListener = onCompleteListener;
    }

    public interface OnCompleteListener {
        void onComplete(String path);
    }
}
/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
