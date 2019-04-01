/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Storage abstraction.
#ifndef INCLUDED_STORAGE_HPP
#define INCLUDED_STORAGE_HPP

#include <set>
#include <string>

#include <Poco/URI.h>
#include <Poco/Util/Application.h>

#include "Auth.hpp"
#include "LOOLWSD.hpp"
#include "Log.hpp"
#include "Util.hpp"
#include "WOPIPermissions.h"
#include <common/Authorization.hpp>

/// Base class of all Storage abstractions.
class StorageBase
{
public:
    /// Represents basic file's attributes.
    /// Used for local and network files.
    class FileInfo
    {
    public:
        FileInfo(const std::string& filename,
                 const std::string& ownerId,
                 const Poco::Timestamp& modifiedTime,
                 size_t /*size*/)
            : _filename(filename),
              _ownerId(ownerId),
              _modifiedTime(modifiedTime)
        {
        }

        bool isValid() const
        {
            // 0-byte files are valid; LO will open them as new docs.
            return !_filename.empty();
        }

        const std::string& getFilename() const { return _filename; }

        const std::string& getOwnerId() const { return _ownerId; }

        void setModifiedTime(const Poco::Timestamp& modifiedTime) { _modifiedTime = modifiedTime; }

        const Poco::Timestamp& getModifiedTime() const { return _modifiedTime; }

    private:
        std::string _filename;
        std::string _ownerId;
        Poco::Timestamp _modifiedTime;
    };

    class SaveResult
    {
    public:
        enum Result
        {
            OK,
            DISKFULL,
            UNAUTHORIZED,
            DOC_CHANGED, /**< Document changed in storage */
            CONFLICT,
            FAILED
        };

        SaveResult(Result result) : _result(result)
        {
        }

        void setResult(Result result)
        {
            _result = result;
        }

        Result getResult() const
        {
            return _result;
        }

        void setSaveAsResult(const std::string& name, const std::string& url)
        {
            _saveAsName = name;
            _saveAsUrl = url;
        }

        const std::string& getSaveAsName() const
        {
            return _saveAsName;
        }

        const std::string& getSaveAsUrl() const
        {
            return _saveAsUrl;
        }

    private:
        Result _result;
        std::string _saveAsName;
        std::string _saveAsUrl;
    };

    enum class LOOLStatusCode
    {
        DOC_CHANGED = 1010 // Document changed externally in storage
    };

    /// localStorePath the absolute root path of the chroot.
    /// jailPath the path within the jail that the child uses.
    StorageBase(const Poco::URI& uri,
                const std::string& localStorePath,
                const std::string& jailPath) :
        _uri(uri),
        _localStorePath(localStorePath),
        _jailPath(jailPath),
        _fileInfo("", "lool", Poco::Timestamp::fromEpochTime(0), 0),
        _isLoaded(false),
        _forceSave(false),
        _isUserModified(false),
        _isAutosave(false)
    {
        LOG_DBG("Storage ctor: " << LOOLWSD::anonymizeUrl(uri.toString()));
    }

    virtual ~StorageBase() {}

    const Poco::URI& getUri() const { return _uri; }

    const std::string getUriString() const { return _uri.toString(); }

    const std::string& getJailPath() const { return _jailPath; };

    /// Returns the root path to the jailed file.
    const std::string& getRootFilePath() const { return _jailedFilePath; };

    /// Set the root path of the jailed file, only for use in cases where we actually have converted
    /// it to another format, in the same directory
    void setRootFilePath(const std::string& newPath)
    {
        // Could assert here that it is in the same directory?
        _jailedFilePath = newPath;
    }

    const std::string& getRootFilePathAnonym() const { return _jailedFilePathAnonym; };

    void setRootFilePathAnonym(const std::string& newPath)
    {
        _jailedFilePathAnonym = newPath;
    }

    void setLoaded(bool loaded) { _isLoaded = loaded; }

    bool isLoaded() const { return _isLoaded; }

    /// Asks the storage object to force overwrite to storage upon next save
    /// even if document turned out to be changed in storage
    void forceSave(bool newSave = true) { _forceSave = newSave; }

    bool getForceSave() const { return _forceSave; }

    /// To be able to set the WOPI extension header appropriately.
    void setUserModified(bool userModified) { _isUserModified = userModified; }

    bool isUserModified() const { return _isUserModified; }

    /// To be able to set the WOPI 'is autosave/is exitsave?' headers appropriately.
    void setIsAutosave(bool isAutosave) { _isAutosave = isAutosave; }
    bool getIsAutosave() const { return _isAutosave; }
    void setIsExitSave(bool exitSave) { _isExitSave = exitSave; }
    bool isExitSave() const { return _isExitSave; }

    void setFileInfo(const FileInfo& fileInfo) { _fileInfo = fileInfo; }

    /// Returns the basic information about the file.
    FileInfo& getFileInfo() { return _fileInfo; }

    std::string getFileExtension() const { return Poco::Path(_fileInfo.getFilename()).getExtension(); }

    /// Returns a local file path for the given URI.
    /// If necessary copies the file locally first.
    virtual std::string loadStorageFileToLocal(const Authorization& auth) = 0;

    /// Writes the contents of the file back to the source.
    /// @param savedFile When the operation was saveAs, this is the path to the file that was saved.
    virtual SaveResult saveLocalFileToStorage(const Authorization& auth, const std::string& saveAsPath, const std::string& saveAsFilename) = 0;

    static size_t getFileSize(const std::string& filename);

    /// Must be called at startup to configure.
    static void initialize();

    /// Storage object creation factory.
    static std::unique_ptr<StorageBase> create(const Poco::URI& uri,
                                               const std::string& jailRoot,
                                               const std::string& jailPath);

    static bool allowedWopiHost(const std::string& host);
protected:

    /// Returns the root path of the jail directory of docs.
    std::string getLocalRootPath() const;

private:
    const Poco::URI _uri;
    std::string _localStorePath;
    std::string _jailPath;
    std::string _jailedFilePath;
    std::string _jailedFilePathAnonym;
    FileInfo _fileInfo;
    bool _isLoaded;
    bool _forceSave;

    /// The document has been modified by the user.
    bool _isUserModified;

    /// This save operation is an autosave.
    bool _isAutosave;
    /// Saving on exit (when the document is cleaned up from memory)
    bool _isExitSave;

    static bool FilesystemEnabled;
    static bool WopiEnabled;
    /// Allowed/denied WOPI hosts, if any and if WOPI is enabled.
    static Util::RegexListMatcher WopiHosts;
};

/// Trivial implementation of local storage that does not need do anything.
class LocalStorage : public StorageBase
{
public:
    LocalStorage(const Poco::URI& uri,
                 const std::string& localStorePath,
                 const std::string& jailPath) :
        StorageBase(uri, localStorePath, jailPath),
        _isCopy(false)
    {
        LOG_INF("LocalStorage ctor with localStorePath: [" << localStorePath <<
                "], jailPath: [" << jailPath << "], uri: [" << LOOLWSD::anonymizeUrl(uri.toString()) << "].");
    }

    class LocalFileInfo
    {
    public:
        LocalFileInfo(const std::string& userId,
                      const std::string& username)
            : _userId(userId),
              _username(username)
        {
        }

        const std::string& getUserId() const { return _userId; }
        const std::string& getUsername() const { return _username; }

    private:
        std::string _userId;
        std::string _username;
    };

    /// Returns the URI specific file data
    /// Also stores the basic file information which can then be
    /// obtained using getFileInfo method
    std::unique_ptr<LocalFileInfo> getLocalFileInfo();

    std::string loadStorageFileToLocal(const Authorization& auth) override;

    SaveResult saveLocalFileToStorage(const Authorization& auth, const std::string& saveAsPath, const std::string& saveAsFilename) override;

private:
    /// True if the jailed file is not linked but copied.
    bool _isCopy;
    static std::atomic<unsigned> LastLocalStorageId;
};

/// WOPI protocol backed storage.
class WopiStorage : public StorageBase
{
public:
    WopiStorage(const Poco::URI& uri,
                const std::string& localStorePath,
                const std::string& jailPath) :
        StorageBase(uri, localStorePath, jailPath),
        _wopiLoadDuration(0)
    {
        LOG_INF("WopiStorage ctor with localStorePath: [" << localStorePath <<
                "], jailPath: [" << jailPath << "], uri: [" << LOOLWSD::anonymizeUrl(uri.toString()) << "].");
    }

    class WOPIFileInfo
    {
    public:
        WOPIFileInfo(const std::string& userid,
                     const std::string& obfuscatedUserId,
                     const std::string& username,
                     const std::string& filename,
                     const std::string& userExtraInfo,
                     const std::string& watermarkText,
                     const std::string& templateSaveAs,
                     const std::string& postMessageOrigin,
                     const bool enableOwnerTermination,
                     const WOPIPermissions::TriState disableChangeTrackingShow,
                     const WOPIPermissions::TriState disableChangeTrackingRecord,
                     const std::shared_ptr<WOPIPermissions>& permissions,
                     const std::chrono::duration<double> callDuration)
            : _userId(userid),
              _obfuscatedUserId(obfuscatedUserId),
              _username(username),
              _filename(filename),
              _watermarkText(watermarkText),
              _templateSaveAs(templateSaveAs),
              _postMessageOrigin(postMessageOrigin),
              _enableOwnerTermination(enableOwnerTermination),
              _disableChangeTrackingShow(disableChangeTrackingShow),
              _disableChangeTrackingRecord(disableChangeTrackingRecord),
              _permissions(permissions),
              _callDuration(callDuration)
            {
                _userExtraInfo = userExtraInfo;
            }

        const std::string& getUserId() const { return _userId; }

        const std::string& getUsername() const { return _username; }

        const std::string& getFilename() const { return _filename; }

        const std::string& getUserExtraInfo() const { return _userExtraInfo; }

        const std::string& getWatermarkText() const { return _watermarkText; }

        const std::string& getTemplateSaveAs() const { return _templateSaveAs; };

        std::string& getPostMessageOrigin() { return _postMessageOrigin; }

        bool getEnableOwnerTermination() const { return _enableOwnerTermination; }

        WOPIPermissions::TriState getDisableChangeTrackingShow() const { return _disableChangeTrackingShow; }

        WOPIPermissions::TriState getDisableChangeTrackingRecord() const { return _disableChangeTrackingRecord; }

        std::shared_ptr<WOPIPermissions>& getPermissions() { return _permissions; }

        std::chrono::duration<double> getCallDuration() const { return _callDuration; }

    private:
        /// User id of the user accessing the file
        std::string _userId;
        /// Obfuscated User id used for logging the UserId.
        std::string _obfuscatedUserId;
        /// Display Name of user accessing the file
        std::string _username;
        /// Display Name of the file user accessing
        std::string _filename;
        /// Extra info per user, typically mail and other links, as json.
        std::string _userExtraInfo;
        /// In case a watermark has to be rendered on each tile.
        std::string _watermarkText;
        /// In case we want to use this file as a template, it should be first re-saved under this name (using PutRelativeFile).
        std::string _templateSaveAs;
        /// WOPI Post message property
        std::string _postMessageOrigin;
        /// If WOPI host has enabled owner termination feature on
        bool _enableOwnerTermination;
        /// If we should disable change-tracking visibility by default (meaningful at loading).
        WOPIPermissions::TriState _disableChangeTrackingShow;
        /// If we should disable change-tracking ability by default (meaningful at loading).
        WOPIPermissions::TriState _disableChangeTrackingRecord;
        /// WOPI Permissions for this user
        std::shared_ptr<WOPIPermissions> _permissions;
        /// Time it took to call WOPI's CheckFileInfo
        std::chrono::duration<double> _callDuration;
    };

    /// Returns the response of CheckFileInfo WOPI call for URI that was
    /// provided during the initial creation of the WOPI storage.
    /// Also extracts the basic file information from the response
    /// which can then be obtained using getFileInfo()
    std::unique_ptr<WOPIFileInfo> getWOPIFileInfo(const Authorization& auth);

    /// uri format: http://server/<...>/wopi*/files/<id>/content
    std::string loadStorageFileToLocal(const Authorization& auth) override;

    SaveResult saveLocalFileToStorage(const Authorization& auth, const std::string& saveAsPath, const std::string& saveAsFilename) override;

    /// Total time taken for making WOPI calls during load
    std::chrono::duration<double> getWopiLoadDuration() const { return _wopiLoadDuration; }

private:
    // Time spend in loading the file from storage
    std::chrono::duration<double> _wopiLoadDuration;
};

/// WebDAV protocol backed storage.
class WebDAVStorage : public StorageBase
{
public:
    WebDAVStorage(const Poco::URI& uri,
                  const std::string& localStorePath,
                  const std::string& jailPath,
                  std::unique_ptr<AuthBase> authAgent) :
        StorageBase(uri, localStorePath, jailPath),
        _authAgent(std::move(authAgent))
    {
        LOG_INF("WebDAVStorage ctor with localStorePath: [" << localStorePath <<
                "], jailPath: [" << jailPath << "], uri: [" << LOOLWSD::anonymizeUrl(uri.toString()) << "].");
    }

    // Implement me
    // WebDAVFileInfo getWebDAVFileInfo(const Poco::URI& uriPublic);

    std::string loadStorageFileToLocal(const Authorization& auth) override;

    SaveResult saveLocalFileToStorage(const Authorization& auth, const std::string& saveAsPath, const std::string& saveAsFilename) override;

private:
    std::unique_ptr<AuthBase> _authAgent;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
