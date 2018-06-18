/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include "Storage.hpp"

#include <algorithm>
#include <cassert>
#include <errno.h>
#include <fstream>
#include <iconv.h>
#include <string>

#include <Poco/DateTime.h>
#include <Poco/DateTimeParser.h>
#include <Poco/Exception.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/DNS.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPSClientSession.h>
#include <Poco/Net/NameValueCollection.h>
#include <Poco/Net/NetworkInterface.h>
#include <Poco/Net/SSLManager.h>
#include <Poco/StreamCopier.h>
#include <Poco/Timestamp.h>

// For residual Poco SSL usage.
#include <Poco/Net/AcceptCertificateHandler.h>
#include <Poco/Net/Context.h>
#include <Poco/Net/KeyConsoleHandler.h>
#include <Poco/Net/SSLManager.h>

#include "Auth.hpp"
#include "Common.hpp"
#include "Exceptions.hpp"
#include "LOOLWSD.hpp"
#include <Log.hpp>
#include <Unit.hpp>
#include <Util.hpp>
#include <common/FileUtil.hpp>
#include <common/JsonUtil.hpp>

using std::size_t;

bool StorageBase::FilesystemEnabled;
bool StorageBase::WopiEnabled;
Util::RegexListMatcher StorageBase::WopiHosts;

std::string StorageBase::getLocalRootPath() const
{
    auto localPath = _jailPath;
    if (localPath[0] == '/')
    {
        // Remove the leading /
        localPath.erase(0, 1);
    }

    // /chroot/jailId/user/doc/childId
    const auto rootPath = Poco::Path(_localStorePath, localPath);
    Poco::File(rootPath).createDirectories();

    return rootPath.toString();
}

size_t StorageBase::getFileSize(const std::string& filename)
{
    return std::ifstream(filename, std::ifstream::ate | std::ifstream::binary).tellg();
}

void StorageBase::initialize()
{
    const auto& app = Poco::Util::Application::instance();
    FilesystemEnabled = app.config().getBool("storage.filesystem[@allow]", false);

    // Parse the WOPI settings.
    WopiHosts.clear();
    WopiEnabled = app.config().getBool("storage.wopi[@allow]", false);
    if (WopiEnabled)
    {
        for (size_t i = 0; ; ++i)
        {
            const std::string path = "storage.wopi.host[" + std::to_string(i) + "]";
            const auto host = app.config().getString(path, "");
            if (!host.empty())
            {
                if (app.config().getBool(path + "[@allow]", false))
                {
                    LOG_INF("Adding trusted WOPI host: [" << host << "].");
                    WopiHosts.allow(host);
                }
                else
                {
                    LOG_INF("Adding blocked WOPI host: [" << host << "].");
                    WopiHosts.deny(host);
                }
            }
            else if (!app.config().has(path))
            {
                break;
            }
        }
    }

#if ENABLE_SSL
    // FIXME: should use our own SSL socket implementation here.
    Poco::Crypto::initializeCrypto();
    Poco::Net::initializeSSL();

    // Init client
    Poco::Net::Context::Params sslClientParams;

    // TODO: Be more strict and setup SSL key/certs for remote server and us
    sslClientParams.verificationMode = Poco::Net::Context::VERIFY_NONE;

    Poco::SharedPtr<Poco::Net::PrivateKeyPassphraseHandler> consoleClientHandler = new Poco::Net::KeyConsoleHandler(false);
    Poco::SharedPtr<Poco::Net::InvalidCertificateHandler> invalidClientCertHandler = new Poco::Net::AcceptCertificateHandler(false);

    Poco::Net::Context::Ptr sslClientContext = new Poco::Net::Context(Poco::Net::Context::CLIENT_USE, sslClientParams);
    Poco::Net::SSLManager::instance().initializeClient(consoleClientHandler, invalidClientCertHandler, sslClientContext);
#endif
}

bool isLocalhost(const std::string& targetHost)
{
    std::string targetAddress;
    try
    {
        targetAddress = Poco::Net::DNS::resolveOne(targetHost).toString();
    }
    catch (const Poco::Exception& exc)
    {
        LOG_WRN("Poco::Net::DNS::resolveOne(\"" << targetHost << "\") failed: " << exc.displayText());
        try
        {
            targetAddress = Poco::Net::IPAddress(targetHost).toString();
        }
        catch (const Poco::Exception& exc1)
        {
            LOG_WRN("Poco::Net::IPAddress(\"" << targetHost << "\") failed: " << exc1.displayText());
        }
    }

    Poco::Net::NetworkInterface::NetworkInterfaceList list = Poco::Net::NetworkInterface::list(true,true);
    for (auto& netif : list)
    {
        std::string address = netif.address().toString();
        address = address.substr(0, address.find('%', 0));
        if (address == targetAddress)
        {
            LOG_INF("WOPI host is on the same host as the WOPI client: \"" <<
                    targetAddress << "\". Connection is allowed.");
            return true;
        }
    }

    LOG_INF("WOPI host is not on the same host as the WOPI client: \"" <<
            targetAddress << "\". Connection is not allowed.");
    return false;
}

std::unique_ptr<StorageBase> StorageBase::create(const Poco::URI& uri, const std::string& jailRoot, const std::string& jailPath)
{
    // FIXME: By the time this gets called we have already sent to the client three
    // 'statusindicator:' messages: 'find', 'connect' and 'ready'. We should ideally do the checks
    // here much earlier. Also, using exceptions is lame and makes understanding the code harder,
    // but that is just my personal preference.

    std::unique_ptr<StorageBase> storage;

    if (UnitWSD::get().createStorage(uri, jailRoot, jailPath, storage))
    {
        LOG_INF("Storage load hooked.");
        if (storage)
        {
            return storage;
        }
    }
    else if (uri.isRelative() || uri.getScheme() == "file")
    {
        LOG_INF("Public URI [" << LOOLWSD::anonymizeUrl(uri.toString()) << "] is a file.");

#if ENABLE_DEBUG
        if (std::getenv("FAKE_UNAUTHORIZED"))
        {
            LOG_FTL("Faking an UnauthorizedRequestException");
            throw UnauthorizedRequestException("No acceptable WOPI hosts found matching the target host in config.");
        }
#endif
        if (FilesystemEnabled)
        {
            return std::unique_ptr<StorageBase>(new LocalStorage(uri, jailRoot, jailPath));
        }
        else
        {
            // guard against attempts to escape
            Poco::URI normalizedUri(uri);
            normalizedUri.normalize();

            std::vector<std::string> pathSegments;
            normalizedUri.getPathSegments(pathSegments);

            if (pathSegments.size() == 4 && pathSegments[0] == "tmp" && pathSegments[1] == "convert-to")
            {
                LOG_INF("Public URI [" << LOOLWSD::anonymizeUrl(normalizedUri.toString()) << "] is actually a convert-to tempfile.");
                return std::unique_ptr<StorageBase>(new LocalStorage(normalizedUri, jailRoot, jailPath));
            }
        }

        LOG_ERR("Local Storage is disabled by default. Enable in the config file or on the command-line to enable.");
    }
    else if (WopiEnabled)
    {
        LOG_INF("Public URI [" << LOOLWSD::anonymizeUrl(uri.toString()) << "] considered WOPI.");
        const auto& targetHost = uri.getHost();
        if (WopiHosts.match(targetHost) || isLocalhost(targetHost))
        {
            return std::unique_ptr<StorageBase>(new WopiStorage(uri, jailRoot, jailPath));
        }
        LOG_ERR("No acceptable WOPI hosts found matching the target host [" << targetHost << "] in config.");
        throw UnauthorizedRequestException("No acceptable WOPI hosts found matching the target host [" + targetHost + "] in config.");
    }

    throw BadRequestException("No Storage configured or invalid URI.");
}

std::atomic<unsigned> LocalStorage::LastLocalStorageId;

std::unique_ptr<LocalStorage::LocalFileInfo> LocalStorage::getLocalFileInfo()
{
    const auto path = Poco::Path(_uri.getPath());
    LOG_DBG("Getting info for local uri [" << LOOLWSD::anonymizeUrl(_uri.toString()) << "], path [" << LOOLWSD::anonymizeUrl(path.toString()) << "].");

    const auto& filename = path.getFileName();
    const auto file = Poco::File(path);
    const auto lastModified = file.getLastModified();
    const auto size = file.getSize();

    _fileInfo = FileInfo({filename, "localhost", lastModified, size});

    // Set automatic userid and username
    return std::unique_ptr<LocalStorage::LocalFileInfo>(new LocalFileInfo({"localhost" + std::to_string(LastLocalStorageId), "Local Host #" + std::to_string(LastLocalStorageId++)}));
}

std::string LocalStorage::loadStorageFileToLocal(const Authorization& /*auth*/)
{
    // /chroot/jailId/user/doc/childId/file.ext
    const auto filename = Poco::Path(_uri.getPath()).getFileName();
    _jailedFilePath = Poco::Path(getLocalRootPath(), filename).toString();
    _jailedFilePathAnonym = LOOLWSD::anonymizeUrl(_jailedFilePath);
    LOG_INF("Public URI [" << LOOLWSD::anonymizeUrl(_uri.getPath()) <<
            "] jailed to [" << _jailedFilePathAnonym << "].");

    // Despite the talk about URIs it seems that _uri is actually just a pathname here
    const auto publicFilePath = _uri.getPath();

    if (!FileUtil::checkDiskSpace(_jailedFilePath))
    {
        throw StorageSpaceLowException("Low disk space for " + _jailedFilePathAnonym);
    }

    LOG_INF("Linking " << LOOLWSD::anonymizeUrl(publicFilePath) << " to " << _jailedFilePathAnonym);
    if (!Poco::File(_jailedFilePath).exists() && link(publicFilePath.c_str(), _jailedFilePath.c_str()) == -1)
    {
        // Failed
        LOG_WRN("link(\"" << LOOLWSD::anonymizeUrl(publicFilePath) << "\", \"" << _jailedFilePathAnonym << "\") failed. Will copy. "
                "Linking error: " << errno << " " << strerror(errno));
    }

    try
    {
        // Fallback to copying.
        if (!Poco::File(_jailedFilePath).exists())
        {
            LOG_INF("Copying " << LOOLWSD::anonymizeUrl(publicFilePath) << " to " << _jailedFilePathAnonym);
            Poco::File(publicFilePath).copyTo(_jailedFilePath);
            _isCopy = true;
        }
    }
    catch (const Poco::Exception& exc)
    {
        LOG_ERR("copyTo(\"" << LOOLWSD::anonymizeUrl(publicFilePath) << "\", \"" << _jailedFilePathAnonym << "\") failed: " << exc.displayText());
        throw;
    }

    _isLoaded = true;
    // Now return the jailed path.
#ifndef KIT_IN_PROCESS
    if (LOOLWSD::NoCapsForKit)
        return _jailedFilePath;
    else
        return Poco::Path(_jailPath, filename).toString();
#else
    return _jailedFilePath;
#endif
}

StorageBase::SaveResult LocalStorage::saveLocalFileToStorage(const Authorization& /*auth*/, const std::string& /*saveAsPath*/, const std::string& /*saveAsFilename*/)
{
    try
    {
        LOG_TRC("Saving local file to local file storage (isCopy: " << _isCopy << ") for " << _jailedFilePathAnonym);
        // Copy the file back.
        if (_isCopy && Poco::File(_jailedFilePath).exists())
        {
            LOG_INF("Copying " << _jailedFilePathAnonym << " to " << LOOLWSD::anonymizeUrl(_uri.getPath()));
            Poco::File(_jailedFilePath).copyTo(_uri.getPath());
        }

        // update its fileinfo object. This is used later to check if someone else changed the
        // document while we are/were editing it
        _fileInfo._modifiedTime = Poco::File(_uri.getPath()).getLastModified();
        LOG_TRC("New FileInfo modified time in storage " << _fileInfo._modifiedTime);
    }
    catch (const Poco::Exception& exc)
    {
        LOG_ERR("copyTo(\"" << _jailedFilePathAnonym << "\", \"" << LOOLWSD::anonymizeUrl(_uri.getPath()) <<
                "\") failed: " << exc.displayText());
        return StorageBase::SaveResult::FAILED;
    }

    return StorageBase::SaveResult(StorageBase::SaveResult::OK);
}

namespace
{

inline
Poco::Net::HTTPClientSession* getHTTPClientSession(const Poco::URI& uri)
{
    // FIXME: if we're configured for http - we can still use an https:// wopi
    // host surely; of course - the converse is not true / sensible.
    return (LOOLWSD::isSSLEnabled() || LOOLWSD::isSSLTermination())
        ? new Poco::Net::HTTPSClientSession(uri.getHost(), uri.getPort(),
                                            Poco::Net::SSLManager::instance().defaultClientContext())
        : new Poco::Net::HTTPClientSession(uri.getHost(), uri.getPort());
}

void addStorageDebugCookie(Poco::Net::HTTPRequest& request)
{
    (void) request;
#if ENABLE_DEBUG
    if (std::getenv("LOOL_STORAGE_COOKIE"))
    {
        Poco::Net::NameValueCollection nvcCookies;
        std::vector<std::string> cookieTokens = LOOLProtocol::tokenize(std::string(std::getenv("LOOL_STORAGE_COOKIE")), ':');
        if (cookieTokens.size() == 2)
        {
            nvcCookies.add(cookieTokens[0], cookieTokens[1]);
            request.setCookies(nvcCookies);
            LOG_TRC("Added storage debug cookie [" << cookieTokens[0] << "=" << cookieTokens[1] << "].");
        }
    }
#endif
}

Poco::Timestamp iso8601ToTimestamp(const std::string& iso8601Time, const std::string& name)
{
    Poco::Timestamp timestamp = Poco::Timestamp::fromEpochTime(0);
    try
    {
        int timeZoneDifferential;
        Poco::DateTime dateTime;
        Poco::DateTimeParser::parse(Poco::DateTimeFormat::ISO8601_FRAC_FORMAT, iso8601Time, dateTime, timeZoneDifferential);
        timestamp = dateTime.timestamp();
    }
    catch (const Poco::SyntaxException& exc)
    {
        LOG_WRN(name << " [" << iso8601Time << "] is in invalid format: " << exc.displayText() <<
                (exc.nested() ? " (" + exc.nested()->displayText() + ")" : "") << ". Returning " << timestamp);
    }

    return timestamp;
}

} // anonymous namespace

std::unique_ptr<WopiStorage::WOPIFileInfo> WopiStorage::getWOPIFileInfo(const Authorization& auth)
{
    // update the access_token to the one matching to the session
    Poco::URI uriObject(_uri);
    auth.authorizeURI(uriObject);
    const std::string uriAnonym = LOOLWSD::anonymizeUrl(uriObject.toString());

    LOG_DBG("Getting info for wopi uri [" << uriAnonym << "].");

    std::string wopiResponse;
    std::chrono::duration<double> callDuration(0);
    try
    {
        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, uriObject.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
        request.set("User-Agent", WOPI_AGENT_STRING);
        auth.authorizeRequest(request);
        addStorageDebugCookie(request);

        const auto startTime = std::chrono::steady_clock::now();

        std::unique_ptr<Poco::Net::HTTPClientSession> psession(getHTTPClientSession(uriObject));
        psession->sendRequest(request);

        Poco::Net::HTTPResponse response;
        std::istream& rs = psession->receiveResponse(response);

        callDuration = (std::chrono::steady_clock::now() - startTime);

        auto logger = Log::trace();
        if (logger.enabled())
        {
            logger << "WOPI::CheckFileInfo header for URI [" << uriAnonym << "]:\n";
            for (const auto& pair : response)
            {
                logger << '\t' << pair.first << ": " << pair.second << " / ";
            }

            LOG_END(logger);
        }

        if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK)
        {
            LOG_ERR("WOPI::CheckFileInfo failed with " << response.getStatus() << ' ' << response.getReason());
            throw StorageConnectionException("WOPI::CheckFileInfo failed");
        }

        Poco::StreamCopier::copyToString(rs, wopiResponse);
    }
    catch (const Poco::Exception& pexc)
    {
        LOG_ERR("Cannot get file info from WOPI storage uri [" << uriAnonym << "]. Error: " <<
                pexc.displayText() << (pexc.nested() ? " (" + pexc.nested()->displayText() + ")" : ""));
        throw;
    }

    // Parse the response.
    std::string filename;
    size_t size = 0;
    std::string ownerId;
    std::string userId;
    std::string userName;
    std::string userExtraInfo;
    std::string watermarkText;
    bool canWrite = false;
    bool enableOwnerTermination = false;
    std::string postMessageOrigin;
    bool hidePrintOption = false;
    bool hideSaveOption = false;
    bool hideExportOption = false;
    bool disablePrint = false;
    bool disableExport = false;
    bool disableCopy = false;
    bool disableInactiveMessages = false;
    std::string lastModifiedTime;
    bool userCanNotWriteRelative = true;
    WOPIFileInfo::TriState disableChangeTrackingRecord = WOPIFileInfo::TriState::Unset;
    WOPIFileInfo::TriState disableChangeTrackingShow = WOPIFileInfo::TriState::Unset;
    WOPIFileInfo::TriState hideChangeTrackingControls = WOPIFileInfo::TriState::Unset;

    Poco::JSON::Object::Ptr object;
    if (JsonUtil::parseJSON(wopiResponse, object))
    {
        JsonUtil::findJSONValue(object, "BaseFileName", filename);
        JsonUtil::findJSONValue(object, "OwnerId", ownerId);
        JsonUtil::findJSONValue(object, "UserId", userId);
        JsonUtil::findJSONValue(object, "UserFriendlyName", userName);

        // Anonymize key values.
        if (LOOLWSD::AnonymizeFilenames || LOOLWSD::AnonymizeUsernames)
        {
            // Set anonymized version of the above fields before logging.
            // Note: anonymization caches the result, so we don't need to store here.
            if (LOOLWSD::AnonymizeFilenames)
                object->set("BaseFileName", LOOLWSD::anonymizeUrl(filename));

            if (LOOLWSD::AnonymizeUsernames)
            {
                object->set("OwnerId", LOOLWSD::anonymizeUsername(ownerId));
                object->set("UserId", LOOLWSD::anonymizeUsername(userId));
                object->set("UserFriendlyName", LOOLWSD::anonymizeUsername(userName));
            }

            std::ostringstream oss;
            object->stringify(oss);
            wopiResponse = oss.str();

            // Remove them for performance reasons; they aren't needed anymore.
            if (LOOLWSD::AnonymizeFilenames)
                object->remove("BaseFileName");

            if (LOOLWSD::AnonymizeUsernames)
            {
                object->remove("OwnerId");
                object->remove("UserId");
                object->remove("UserFriendlyName");
            }
        }

        // Log either an original or anonymized version, depending on anonymization flags.
        LOG_DBG("WOPI::CheckFileInfo (" << callDuration.count() * 1000. << " ms): " << wopiResponse);

        JsonUtil::findJSONValue(object, "Size", size);
        JsonUtil::findJSONValue(object, "UserExtraInfo", userExtraInfo);
        JsonUtil::findJSONValue(object, "WatermarkText", watermarkText);
        JsonUtil::findJSONValue(object, "UserCanWrite", canWrite);
        JsonUtil::findJSONValue(object, "PostMessageOrigin", postMessageOrigin);
        JsonUtil::findJSONValue(object, "HidePrintOption", hidePrintOption);
        JsonUtil::findJSONValue(object, "HideSaveOption", hideSaveOption);
        JsonUtil::findJSONValue(object, "HideExportOption", hideExportOption);
        JsonUtil::findJSONValue(object, "EnableOwnerTermination", enableOwnerTermination);
        JsonUtil::findJSONValue(object, "DisablePrint", disablePrint);
        JsonUtil::findJSONValue(object, "DisableExport", disableExport);
        JsonUtil::findJSONValue(object, "DisableCopy", disableCopy);
        JsonUtil::findJSONValue(object, "DisableInactiveMessages", disableInactiveMessages);
        JsonUtil::findJSONValue(object, "LastModifiedTime", lastModifiedTime);
        JsonUtil::findJSONValue(object, "UserCanNotWriteRelative", userCanNotWriteRelative);
        bool booleanFlag = false;
        if (JsonUtil::findJSONValue(object, "DisableChangeTrackingRecord", booleanFlag))
            disableChangeTrackingRecord = (booleanFlag ? WOPIFileInfo::TriState::True : WOPIFileInfo::TriState::False);
        if (JsonUtil::findJSONValue(object, "DisableChangeTrackingShow", booleanFlag))
            disableChangeTrackingShow = (booleanFlag ? WOPIFileInfo::TriState::True : WOPIFileInfo::TriState::False);
        if (JsonUtil::findJSONValue(object, "HideChangeTrackingControls", booleanFlag))
            hideChangeTrackingControls = (booleanFlag ? WOPIFileInfo::TriState::True : WOPIFileInfo::TriState::False);
    }
    else
    {
        if (LOOLWSD::AnonymizeFilenames || LOOLWSD::AnonymizeUsernames)
            LOG_ERR("WOPI::CheckFileInfo failed or no valid JSON payload returned. Access denied.");
        else
            LOG_ERR("WOPI::CheckFileInfo failed or no valid JSON payload returned. Access denied. "
                    "Original response: [" << wopiResponse << "].");
        throw UnauthorizedRequestException("Access denied. WOPI::CheckFileInfo failed on: " + uriAnonym);
    }

    const Poco::Timestamp modifiedTime = iso8601ToTimestamp(lastModifiedTime, "LastModifiedTime");
    _fileInfo = FileInfo({filename, ownerId, modifiedTime, size});

    return std::unique_ptr<WopiStorage::WOPIFileInfo>(new WOPIFileInfo(
        {userId, userName, userExtraInfo, watermarkText, canWrite,
         postMessageOrigin, hidePrintOption, hideSaveOption, hideExportOption,
         enableOwnerTermination, disablePrint, disableExport, disableCopy,
         disableInactiveMessages, userCanNotWriteRelative,
         disableChangeTrackingShow, disableChangeTrackingRecord,
         hideChangeTrackingControls, callDuration}));
}

/// uri format: http://server/<...>/wopi*/files/<id>/content
std::string WopiStorage::loadStorageFileToLocal(const Authorization& auth)
{
    // WOPI URI to download files ends in '/contents'.
    // Add it here to get the payload instead of file info.
    Poco::URI uriObject(_uri);
    uriObject.setPath(uriObject.getPath() + "/contents");
    auth.authorizeURI(uriObject);
    const std::string uriAnonym = LOOLWSD::anonymizeUrl(uriObject.toString());

    LOG_DBG("Wopi requesting: " << uriAnonym);

    const auto startTime = std::chrono::steady_clock::now();
    try
    {
        std::unique_ptr<Poco::Net::HTTPClientSession> psession(getHTTPClientSession(uriObject));

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_GET, uriObject.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
        request.set("User-Agent", WOPI_AGENT_STRING);
        auth.authorizeRequest(request);
        addStorageDebugCookie(request);
        psession->sendRequest(request);

        Poco::Net::HTTPResponse response;
        std::istream& rs = psession->receiveResponse(response);
        const std::chrono::duration<double> diff = (std::chrono::steady_clock::now() - startTime);
        _wopiLoadDuration += diff;

        auto logger = Log::trace();
        if (logger.enabled())
        {
            logger << "WOPI::GetFile header for URI [" << uriAnonym << "]:\n";
            for (const auto& pair : response)
            {
                logger << '\t' << pair.first << ": " << pair.second << " / ";
            }

            LOG_END(logger);
        }

        if (response.getStatus() != Poco::Net::HTTPResponse::HTTP_OK)
        {
            LOG_ERR("WOPI::GetFile failed with " << response.getStatus() << ' ' << response.getReason());
            throw StorageConnectionException("WOPI::GetFile failed");
        }
        else // Successful
        {
            _jailedFilePath = Poco::Path(getLocalRootPath(), _fileInfo._filename).toString();
            _jailedFilePathAnonym = LOOLWSD::anonymizeUrl(_jailedFilePath);
            std::ofstream ofs(_jailedFilePath);
            std::copy(std::istreambuf_iterator<char>(rs),
                      std::istreambuf_iterator<char>(),
                      std::ostreambuf_iterator<char>(ofs));
            ofs.close();
            LOG_INF("WOPI::GetFile downloaded " << getFileSize(_jailedFilePath) << " bytes from [" <<
                    uriAnonym << "] -> " << _jailedFilePathAnonym << " in " << diff.count() << "s");

            _isLoaded = true;
            // Now return the jailed path.
            return Poco::Path(_jailPath, _fileInfo._filename).toString();
        }
    }
    catch(const Poco::Exception& pexc)
    {
        LOG_ERR("Cannot load document from WOPI storage uri [" + uriAnonym + "]. Error: " <<
                pexc.displayText() << (pexc.nested() ? " (" + pexc.nested()->displayText() + ")" : ""));
        throw;
    }

    return "";
}

StorageBase::SaveResult WopiStorage::saveLocalFileToStorage(const Authorization& auth, const std::string& saveAsPath, const std::string& saveAsFilename)
{
    // TODO: Check if this URI has write permission (canWrite = true)

    const bool isSaveAs = !saveAsPath.empty() && !saveAsFilename.empty();
    const std::string filePath(isSaveAs ? saveAsPath : _jailedFilePath);
    const std::string filePathAnonym = LOOLWSD::anonymizeUrl(filePath);

    const auto size = getFileSize(filePath);

    Poco::URI uriObject(_uri);
    uriObject.setPath(isSaveAs? uriObject.getPath(): uriObject.getPath() + "/contents");
    auth.authorizeURI(uriObject);
    const std::string uriAnonym = LOOLWSD::anonymizeUrl(uriObject.toString());

    LOG_INF("Uploading URI via WOPI [" << uriAnonym << "] from [" << filePathAnonym + "].");

    StorageBase::SaveResult saveResult(StorageBase::SaveResult::FAILED);
    try
    {
        std::unique_ptr<Poco::Net::HTTPClientSession> psession(getHTTPClientSession(uriObject));

        Poco::Net::HTTPRequest request(Poco::Net::HTTPRequest::HTTP_POST, uriObject.getPathAndQuery(), Poco::Net::HTTPMessage::HTTP_1_1);
        request.set("User-Agent", WOPI_AGENT_STRING);
        auth.authorizeRequest(request);

        if (!isSaveAs)
        {
            // normal save
            request.set("X-WOPI-Override", "PUT");
            request.set("X-LOOL-WOPI-IsModifiedByUser", _isUserModified? "true": "false");
            request.set("X-LOOL-WOPI-IsAutosave", _isAutosave? "true": "false");

            if (!_forceSave)
            {
                // Request WOPI host to not overwrite if timestamps mismatch
                request.set("X-LOOL-WOPI-Timestamp",
                            Poco::DateTimeFormatter::format(Poco::DateTime(_fileInfo._modifiedTime),
                                                            Poco::DateTimeFormat::ISO8601_FRAC_FORMAT));
            }
        }
        else
        {
            // save as
            request.set("X-WOPI-Override", "PUT_RELATIVE");

            // the suggested target has to be in UTF-7; default to extension
            // only when the conversion fails
            std::string suggestedTarget = "." + Poco::Path(saveAsFilename).getExtension();

            iconv_t cd = iconv_open("UTF-7", "UTF-8");
            if (cd == (iconv_t) -1)
                LOG_ERR("Failed to initialize iconv for UTF-7 conversion, using '" << suggestedTarget << "'.");
            else
            {
                std::vector<char> input(saveAsFilename.begin(), saveAsFilename.end());
                std::vector<char> buffer(8 * saveAsFilename.size());

                char* in = &input[0];
                size_t in_left = input.size();
                char* out = &buffer[0];
                size_t out_left = buffer.size();

                if (iconv(cd, &in, &in_left, &out, &out_left) == (size_t) -1)
                    LOG_ERR("Failed to convert '" << saveAsFilename << "' to UTF-7, using '" << suggestedTarget << "'.");
                else
                {
                    // conversion succeeded
                    suggestedTarget = std::string(&buffer[0], buffer.size() - out_left);
                    LOG_TRC("Converted '" << saveAsFilename << "' to UTF-7 as '" << suggestedTarget << "'.");
                }
            }

            request.set("X-WOPI-SuggestedTarget", suggestedTarget);

            request.set("X-WOPI-Size", std::to_string(size));
        }

        request.setContentType("application/octet-stream");
        request.setContentLength(size);
        addStorageDebugCookie(request);
        std::ostream& os = psession->sendRequest(request);

        std::ifstream ifs(filePath);
        Poco::StreamCopier::copyStream(ifs, os);

        Poco::Net::HTTPResponse response;
        std::istream& rs = psession->receiveResponse(response);

        std::ostringstream oss;
        Poco::StreamCopier::copyStream(rs, oss);
        std::string responseString = oss.str();

        const std::string wopiLog(isSaveAs ? "WOPI::PutRelativeFile" : "WOPI::PutFile");

        if (Log::infoEnabled())
        {
            if (LOOLWSD::AnonymizeFilenames)
            {
                Poco::JSON::Object::Ptr object;
                if (JsonUtil::parseJSON(responseString, object))
                {
                    // Anonymize the filename
                    std::string filename;
                    JsonUtil::findJSONValue(object, "Name", filename);
                    object->set("Name", LOOLWSD::anonymizeUsername(filename));
                    // Stringify to log.
                    std::ostringstream ossResponse;
                    object->stringify(ossResponse);
                    responseString = ossResponse.str();
                }
            }

            LOG_INF(wopiLog << " response: " << responseString);
            LOG_INF(wopiLog << " uploaded " << size << " bytes from [" << filePathAnonym <<
                    "] -> [" << uriAnonym << "]: " << response.getStatus() << " " << response.getReason());
        }

        if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_OK)
        {
            saveResult.setResult(StorageBase::SaveResult::OK);
            Poco::JSON::Object::Ptr object;
            if (JsonUtil::parseJSON(oss.str(), object))
            {
                const std::string lastModifiedTime = JsonUtil::getJSONValue<std::string>(object, "LastModifiedTime");
                LOG_TRC(wopiLog << " returns LastModifiedTime [" << lastModifiedTime << "].");
                _fileInfo._modifiedTime = iso8601ToTimestamp(lastModifiedTime, "LastModifiedTime");

                if (isSaveAs)
                {
                    const std::string name = JsonUtil::getJSONValue<std::string>(object, "Name");
                    LOG_TRC(wopiLog << " returns Name [" << LOOLWSD::anonymizeUrl(name) << "].");

                    const std::string url = JsonUtil::getJSONValue<std::string>(object, "Url");
                    LOG_TRC(wopiLog << " returns Url [" << LOOLWSD::anonymizeUrl(url) << "].");

                    saveResult.setSaveAsResult(name, url);
                }

                // Reset the force save flag now, if any, since we are done saving
                // Next saves shouldn't be saved forcefully unless commanded
                _forceSave = false;
            }
            else
            {
                LOG_WRN("Invalid or missing JSON in " << wopiLog << " HTTP_OK response.");
            }
        }
        else if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_REQUESTENTITYTOOLARGE)
        {
            saveResult.setResult(StorageBase::SaveResult::DISKFULL);
        }
        else if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_UNAUTHORIZED)
        {
            saveResult.setResult(StorageBase::SaveResult::UNAUTHORIZED);
        }
        else if (response.getStatus() == Poco::Net::HTTPResponse::HTTP_CONFLICT)
        {
            saveResult.setResult(StorageBase::SaveResult::CONFLICT);
            Poco::JSON::Object::Ptr object;
            if (JsonUtil::parseJSON(oss.str(), object))
            {
                const unsigned loolStatusCode = JsonUtil::getJSONValue<unsigned>(object, "LOOLStatusCode");
                if (loolStatusCode == static_cast<unsigned>(LOOLStatusCode::DOC_CHANGED))
                {
                    saveResult.setResult(StorageBase::SaveResult::DOC_CHANGED);
                }
            }
            else
            {
                LOG_WRN("Invalid or missing JSON in " << wopiLog << " HTTP_CONFLICT response.");
            }
        }
    }
    catch(const Poco::Exception& pexc)
    {
        LOG_ERR("Cannot save file to WOPI storage uri [" << uriAnonym << "]. Error: " <<
                pexc.displayText() << (pexc.nested() ? " (" + pexc.nested()->displayText() + ")" : ""));
        saveResult.setResult(StorageBase::SaveResult::FAILED);
    }

    return saveResult;
}

std::string WebDAVStorage::loadStorageFileToLocal(const Authorization& /*auth*/)
{
    // TODO: implement webdav GET.
    _isLoaded = true;
    return _uri.toString();
}

StorageBase::SaveResult WebDAVStorage::saveLocalFileToStorage(const Authorization& /*auth*/, const std::string& /*saveAsPath*/, const std::string& /*saveAsFilename*/)
{
    // TODO: implement webdav PUT.
    return StorageBase::SaveResult(StorageBase::SaveResult::OK);
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
