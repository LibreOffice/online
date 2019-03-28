/*
 *
 *  * This file is part of the LibreOffice project.
 *  * This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *  * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 */

package org.libreoffice.androidapp;

import android.annotation.SuppressLint;
import android.app.AlertDialog;
import android.app.Dialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.text.Html;
import android.text.Spanned;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.fragment.app.DialogFragment;

public class AboutDialogFragment extends DialogFragment {

    private static final String DEFAULT_DOC_PATH = "/assets/example.odt";


    @NonNull
    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {

        @SuppressLint("InflateParams") //suppressed because the view will be placed in a dialog
        View messageView = getActivity().getLayoutInflater().inflate(R.layout.about, null, false);

        // When linking text, force to always use default color. This works
        // around a pressed color state bug.
        TextView textView = messageView.findViewById(R.id.about_credits);
        int defaultColor = textView.getTextColors().getDefaultColor();
        textView.setTextColor(defaultColor);

        // Take care of placeholders in the version and vendor text views.
        TextView versionView = messageView.findViewById(R.id.about_version);
        TextView vendorView = messageView.findViewById(R.id.about_vendor);
        try
        {
            String versionName = getActivity().getPackageManager()
                    .getPackageInfo(getActivity().getPackageName(), 0).versionName;
            String[] tokens = versionName.split("/");
            if (tokens.length == 3)
            {
                String version = String.format(versionView.getText().toString().replace("\n", "<br/>"),
                        tokens[0], "<a href=\"https://hub.libreoffice.org/git-core/" + tokens[1] + "\">" + tokens[1] + "</a>");
                @SuppressWarnings("deprecation") // since 24 with additional option parameter
                Spanned versionString = Html.fromHtml(version);
                versionView.setText(versionString);
                versionView.setMovementMethod(LinkMovementMethod.getInstance());
                String vendor = vendorView.getText().toString();
                vendor = vendor.replace("$VENDOR", tokens[2]);
                vendorView.setText(vendor);
            }
            else
                throw new PackageManager.NameNotFoundException();
        }
        catch (PackageManager.NameNotFoundException e)
        {
            versionView.setText("");
            vendorView.setText("");
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(getActivity());
        builder .setIcon(R.drawable.lo_icon)
                .setTitle(R.string.app_name)
                .setView(messageView)
                .setNegativeButton(R.string.about_license, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        Intent intent = new Intent(getContext(), ShowHTMLActivity.class);
                        intent.putExtra("path","license.html");
                        startActivity(intent);
                        dialog.dismiss();                    }
                })
                .setPositiveButton(R.string.about_notice, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        Intent intent = new Intent(getContext(), ShowHTMLActivity.class);
                        intent.putExtra("path","notice.txt");
                        startActivity(intent);
                        dialog.dismiss();
                    }
                })
                .setNeutralButton(R.string.about_moreinfo, new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(DialogInterface dialog, int id) {
                        Intent intent = new Intent(getContext(), MainActivity.class);
                        intent.putExtra("URI","file:///android_asset/example.odt");
                        startActivity(intent);
                        dialog.dismiss();
                    }
                });

        return builder.create();
    }


}
