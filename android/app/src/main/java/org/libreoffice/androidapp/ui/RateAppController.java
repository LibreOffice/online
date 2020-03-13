/* -*- tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

package org.libreoffice.androidapp.ui;

import java.text.SimpleDateFormat;
import java.util.Date;

public class RateAppController {
    private static String RATE_ASK_COUNTER_KEY = "RATE_ASK_COUNTER";
    private static String RATE_COUNTER_LAST_UPDATE_KEY = "RATE_COUNTER_LAST_UPDATE_DATE";
    /** 0=NEVER ASKED, 1=POSTPONED, 2=RATED */
    private static String RATE_ASK_STATUS_KEY = "RATE_ASK_STATUS";
    LibreOfficeUIActivity mActivity;
    private int counter;
    private Date lastDate;
    private int status;

    RateAppController(LibreOfficeUIActivity activity) {
        this.mActivity = activity;

        if (mActivity.getPrefs().getInt(RateAppController.RATE_ASK_STATUS_KEY, -1) == -1) {
            /** first time init */
            Date date = new Date();
            mActivity.getPrefs().edit().putLong(RateAppController.RATE_COUNTER_LAST_UPDATE_KEY,  date.getTime()).apply();
            mActivity.getPrefs().edit().putInt(RateAppController.RATE_ASK_STATUS_KEY, 0).apply();
            mActivity.getPrefs().edit().putInt(RateAppController.RATE_ASK_COUNTER_KEY, 0).apply();
            this.counter = 0;
            this.lastDate = date;
            this.status = 0;
        } else {
            this.status = mActivity.getPrefs().getInt(RateAppController.RATE_ASK_STATUS_KEY, 0);
            this.counter = mActivity.getPrefs().getInt(RateAppController.RATE_ASK_COUNTER_KEY, 0);
            long dateTime = mActivity.getPrefs().getLong(RateAppController.RATE_COUNTER_LAST_UPDATE_KEY,  0);
            this.lastDate = new Date(dateTime);
        }
    }

    /**
     *  The counter is incremented once in each day when a document is opened successfully
     * If the counter is modulo of 4 (meaning 5 days, starting from 0) return true unless the user is already rated
     * When dismissed ask for it in another 5 days
     */
    public boolean shouldAsk() {
        SimpleDateFormat dateFormat = new SimpleDateFormat("yyyyMMdd");
        Date today = new Date();

        if (this.status != 2 && !dateFormat.format(today).equals(dateFormat.format(this.lastDate))) {
                if ((this.counter != 0) && (this.counter % 4 == 0)) {
                    updateStatus(false);
                    return true;
                } else
                    updateCounter();
        }
        return false;
    }


    private void updateCounter() {
        this.counter = ++this.counter % 5;
        mActivity.getPrefs().edit().putInt(RateAppController.RATE_ASK_COUNTER_KEY, this.counter).apply();
        updateDate();
    }

    private void updateDate() {
        Date date = new Date();
        this.lastDate = date;
        mActivity.getPrefs().edit().putLong(RateAppController.RATE_COUNTER_LAST_UPDATE_KEY, this.lastDate.getTime()).apply();

    }

    public void updateStatus(boolean isRated) {
        if (!isRated) {
            this.status = 1;
            mActivity.getPrefs().edit().putInt(RateAppController.RATE_ASK_COUNTER_KEY, this.counter).apply();
        } else {
            this.status = 2;
            mActivity.getPrefs().edit().putInt(RateAppController.RATE_ASK_STATUS_KEY, this.status).apply();
        }
        updateCounter();
    }
}
