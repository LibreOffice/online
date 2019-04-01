/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
class WOPIPermissions
{
    public:
    enum class TriState
    {
        False,
        True,
        Unset
    };
    WOPIPermissions(const bool userCanWrite,
                    const bool hidePrintOption,
                    const bool hideSaveOption,
                    const bool hideExportOption,
                    const bool disablePrint,
                    const bool disableExport,
                    const bool disableCopy,
                    const bool disableInactiveMessages,
                    const bool userCanNotWriteRelative,
                    const bool enableInsertRemoteImage,
                    const bool enableShare,
                    const std::string& hideUserList,
                    const TriState hideChangeTrackingControls)
                : _userCanWrite(userCanWrite),
                  _hidePrintOption(hidePrintOption),
                  _hideSaveOption(hideSaveOption),
                  _hideExportOption(hideExportOption),
                  _disablePrint(disablePrint),
                  _disableExport(disableExport),
                  _disableCopy(disableCopy),
                  _disableInactiveMessages(disableInactiveMessages),
                  _userCanNotWriteRelative(userCanNotWriteRelative),
                  _enableInsertRemoteImage(enableInsertRemoteImage),
                  _enableShare(enableShare),
                  _hideUserList(hideUserList),
                  _hideChangeTrackingControls(hideChangeTrackingControls)
                  {}

    bool getUserCanWrite() const { return _userCanWrite; };

    bool getHidePrintOption() const { return _hidePrintOption; }

    void setHidePrintOption(bool hidePrintOption) { _hidePrintOption = hidePrintOption; }

    bool getHideSaveOption() const { return _hideSaveOption; } 

    bool getHideExportOption() const { return _hideExportOption; } 

    void setHideExportOption(bool hideExportOption) { _hideExportOption = hideExportOption; }

    bool getDisablePrint() const { return _disablePrint; }

    bool getDisableExport() const { return _disableExport; }

    bool getDisableCopy() const { return _disableCopy; }

    bool getUserCanNotWriteRelative() const { return _userCanNotWriteRelative; }

    bool getDisableInactiveMessages() const { return _disableInactiveMessages; }

    bool getEnableInsertRemoteImage() const { return _enableInsertRemoteImage; }

    bool getEnableShare() const { return _enableShare; }

    std::string& getHideUserList() { return _hideUserList; }

    TriState getHideChangeTrackingControls() const { return _hideChangeTrackingControls; }

    private:
    /// If user accessing the file has write permission
    bool _userCanWrite;
    /// Hide print button from UI
    bool _hidePrintOption;
    /// Hide save button from UI
    bool _hideSaveOption;
    /// Hide 'Download as' button/menubar item from UI
    bool _hideExportOption;
    /// If WOPI host has allowed the user to print the document
    bool _disablePrint;
    /// If WOPI host has allowed the user to export the document
    bool _disableExport;
    /// If WOPI host has allowed the user to copy to/from the document
    bool _disableCopy;
    /// If WOPI host has allowed the loleaflet to show texts on the overlay informing about inactivity, or if the integration is handling that.
    bool _disableInactiveMessages;
    /// If set to false, users can access the save-as functionality
    bool _userCanNotWriteRelative;
    /// If set to true, users can access the insert remote image functionality
    bool _enableInsertRemoteImage;
    /// If set to true, users can access the file share functionality
    bool _enableShare;
    /// If set to "true", user list on the status bar will be hidden
    /// If set to "mobile" | "tablet" | "desktop", will be hidden on a specified device
    /// (may be joint, delimited by commas eg. "mobile,tablet")
    std::string _hideUserList;
    /// If we should hide change-tracking commands for this user
    TriState _hideChangeTrackingControls;
};
