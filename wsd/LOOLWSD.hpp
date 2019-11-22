/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_LOOLWSD_HPP
#define INCLUDED_LOOLWSD_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <map>
#include <set>
#include <string>

#include <Poco/Path.h>
#include <Poco/Process.h>
#include <Poco/Util/AbstractConfiguration.h>
#include <Poco/Util/OptionSet.h>
#include <Poco/Util/ServerApplication.h>

#include "Util.hpp"

class ChildProcess;
class TraceFileWriter;
class DocumentBroker;
class ClipboardCache;

std::shared_ptr<ChildProcess> getNewChild_Blocks(
#if MOBILEAPP
                                                 const std::string& uri
#endif
                                                 );

/// The Server class which is responsible for all
/// external interactions.
class LOOLWSD : public Poco::Util::ServerApplication
{
public:
    LOOLWSD();
    ~LOOLWSD();

    // An Application is a singleton anyway,
    // so just keep these as statics.
    static std::atomic<uint64_t> NextConnectionId;
    static unsigned int NumPreSpawnedChildren;
    static bool NoCapsForKit;
    static bool NoSeccomp;
    static bool AdminEnabled;
    static std::atomic<int> ForKitWritePipe;
    static std::atomic<int> ForKitProcId;
    static bool DummyLOK;
    static std::string FuzzFileName;
    static std::string ConfigFile;
    static std::string ConfigDir;
    static std::string SysTemplate;
    static std::string LoTemplate;
    static std::string ChildRoot;
    static std::string ServerName;
    static std::string FileServerRoot;
    static std::string ServiceRoot; ///< There are installations that need prefixing every page with some path.
    static std::string LOKitVersion;
    static std::string HostIdentifier; ///< A unique random hash that identifies this server
    static std::string LogLevel;
    static bool AnonymizeUserData;
    static std::uint64_t AnonymizationSalt;
    static std::atomic<unsigned> NumConnections;
    static std::unique_ptr<TraceFileWriter> TraceDumper;
#if !MOBILEAPP
    static std::unique_ptr<ClipboardCache> SavedClipboards;
#endif
    static std::set<std::string> EditFileExtensions;
    static unsigned MaxConnections;
    static unsigned MaxDocuments;
    static std::string OverrideWatermark;
    static std::set<const Poco::Util::AbstractConfiguration*> PluginConfigurations;
    static std::chrono::time_point<std::chrono::system_clock> StartTime;

    /// For testing only [!]
    static std::vector<int> getKitPids();
    /// For testing only [!] DocumentBrokers are mostly single-threaded with their own thread
    static std::vector<std::shared_ptr<DocumentBroker>> getBrokersTestOnly();

    static std::string GetConnectionId()
    {
        return Util::encodeId(NextConnectionId++, 3);
    }

    static bool isSSLEnabled()
    {
#if ENABLE_SSL
        return LOOLWSD::SSLEnabled.get();
#else
        return false;
#endif
    }

    static bool isSSLTermination()
    {
#if ENABLE_SSL
        return LOOLWSD::SSLTermination.get();
#else
        return false;
#endif
    }

    /// Return true iff extension is marked as view action in discovery.xml.
    static bool IsViewFileExtension(const std::string& extension)
    {
#if MOBILEAPP
        (void) extension;
        return false; // mark everything editable on mobile
#else
        std::string lowerCaseExtension = extension;
        std::transform(lowerCaseExtension.begin(), lowerCaseExtension.end(), lowerCaseExtension.begin(), ::tolower);
        return EditFileExtensions.find(lowerCaseExtension) == EditFileExtensions.end();
#endif
    }

    /// Returns the value of the specified application configuration,
    /// or the default, if one doesn't exist.
    template<typename T>
    static
    T getConfigValue(const std::string& name, const T def)
    {
        return getConfigValue(Application::instance().config(), name, def);
    }

    /// Reads and processes path entries with the given property
    /// from the configuration.
    /// Converts relative paths to absolute.
    static
    std::string getPathFromConfig(const std::string& name)
    {
        return getPathFromConfig(Application::instance().config(), name);
    }

    /// Reads and processes path entries with the given property
    /// from the configuration. If value is empty then it reads from fallback
    /// Converts relative paths to absolute.
    static
    std::string getPathFromConfigWithFallback(const std::string& name, const std::string& fallbackName)
    {
        std::string value = LOOLWSD::getPathFromConfig(name);
        if (value.empty())
            return LOOLWSD::getPathFromConfig(fallbackName);
        return value;
    }

    /// Trace a new session and take a snapshot of the file.
    static void dumpNewSessionTrace(const std::string& id, const std::string& sessionId, const std::string& uri, const std::string& path);

    /// Trace the end of a session.
    static void dumpEndSessionTrace(const std::string& id, const std::string& sessionId, const std::string& uri);

    static void dumpEventTrace(const std::string& id, const std::string& sessionId, const std::string& data);

    static void dumpIncomingTrace(const std::string& id, const std::string& sessionId, const std::string& data);

    static void dumpOutgoingTrace(const std::string& id, const std::string& sessionId, const std::string& data);

    /// Waits on Forkit and reaps if it dies, then restores.
    /// Return true if wait succeeds.
    static bool checkAndRestoreForKit();

    /// Creates a new instance of Forkit.
    /// Return true when successfull.
    static bool createForKit();

    /// Checks forkit (and respawns), rebalances
    /// child kit processes and cleans up DocBrokers.
    static void doHousekeeping();

    /// Close document with @docKey and a @message
    static void closeDocument(const std::string& docKey, const std::string& message);

    /// Autosave a given document
    static void autoSave(const std::string& docKey);

    /// Anonymize the basename of filenames, preserving the path and extension.
    static std::string anonymizeUrl(const std::string& url)
    {
        return AnonymizeUserData ? Util::anonymizeUrl(url, AnonymizationSalt) : url;
    }

    /// Anonymize user names and IDs.
    /// Will use the Obfuscated User ID if one is provied via WOPI.
    static std::string anonymizeUsername(const std::string& username)
    {
        return AnonymizeUserData ? Util::anonymize(username, AnonymizationSalt) : username;
    }

    /// get correct server URL with protocol + port number for this running server
    static std::string getServerURL();

    static std::string getVersionJSON();

    int innerMain();

protected:
    void initialize(Poco::Util::Application& self) override;
    void defineOptions(Poco::Util::OptionSet& options) override;
    void handleOption(const std::string& name, const std::string& value) override;
    int main(const std::vector<std::string>& args) override;

    /// Handle various global static destructors.
    void cleanup();

private:
#if ENABLE_SSL
    static Util::RuntimeConstant<bool> SSLEnabled;
    static Util::RuntimeConstant<bool> SSLTermination;
#endif

    void initializeSSL();
    void displayHelp();

    class ConfigValueGetter
    {
        Poco::Util::LayeredConfiguration& _config;
        const std::string& _name;

    public:
        ConfigValueGetter(Poco::Util::LayeredConfiguration& config,
                          const std::string& name)
            : _config(config)
            , _name(name)
        {
        }

        void operator()(int& value) { value = _config.getInt(_name); }
        void operator()(unsigned int& value) { value = _config.getUInt(_name); }
        void operator()(uint64_t& value) { value = _config.getUInt64(_name); }
        void operator()(bool& value) { value = _config.getBool(_name); }
        void operator()(std::string& value) { value = _config.getString(_name); }
        void operator()(double& value) { value = _config.getDouble(_name); }
    };

    template <typename T>
    static bool getSafeConfig(Poco::Util::LayeredConfiguration& config,
                              const std::string& name, T& value)
    {
        try
        {
            ConfigValueGetter(config, name)(value);
            return true;
        }
        catch (const std::exception&)
        {
        }

        return false;
    }

    template<typename T>
    static
    T getConfigValue(Poco::Util::LayeredConfiguration& config,
                     const std::string& name, const T def)
    {
        T value = def;
        if (getSafeConfig(config, name, value) ||
            getSafeConfig(config, name + "[@default]", value))
        {
            return value;
        }

        return def;
    }

    /// Reads and processes path entries with the given property
    /// from the configuration.
    /// Converts relative paths to absolute.
    static
    std::string getPathFromConfig(Poco::Util::LayeredConfiguration& config, const std::string& property)
    {
        std::string path = config.getString(property);
        if (path.empty() && config.hasProperty(property + "[@default]"))
        {
            // Use the default value if empty and a default provided.
            path = config.getString(property + "[@default]");
        }

        // Reconstruct absolute path if relative.
        if (!Poco::Path(path).isAbsolute() &&
            config.hasProperty(property + "[@relative]") &&
            config.getBool(property + "[@relative]"))
        {
            path = Poco::Path(Application::instance().commandPath()).parent().append(path).toString();
        }

        return path;
    }

private:
    /// Settings passed from the command-line to override those in the config file.
    std::map<std::string, std::string> _overrideSettings;

#if MOBILEAPP
public:
    static int prisonerServerSocketFD;
#endif
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
