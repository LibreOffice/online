/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <config.h>

#include "WOPISec.hpp"
#include "LOOLWSD.hpp"

#include <cstdlib>
#include <string>

#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>
#include <Poco/BinaryWriter.h>
#include <Poco/Crypto/RSADigestEngine.h>
#include <Poco/Crypto/RSAKey.h>
#include <Poco/Dynamic/Var.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/LineEndingConverter.h>
#include <Poco/Net/HTTPClientSession.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/NetException.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Timestamp.h>
#include <Poco/URI.h>
#include <Poco/Util/Application.h>

#include <Log.hpp>
#include <Util.hpp>

// Returns .Net tick (=100ns) count since 0001-01-01 00:00:00 Z
// See https://docs.microsoft.com/en-us/dotnet/api/system.datetime.ticks
int64_t DotNetTicks(const std::chrono::system_clock::time_point& utc)
{
    // Get time point for Unix epoch; unfortunately from_time_t isn't constexpr
    const auto aUnxEpoch(std::chrono::system_clock::from_time_t(0));
    const auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(utc - aUnxEpoch);
    return duration_ns.count() / 100 + 621355968000000000;
}

// See http://www.wictorwilen.se/sharepoint-2013-building-your-own-wopi-client-part-2
std::string GetWopiProof(const std::string& access_token, const std::string& uri, int64_t ticks)
{
    std::string decoded_access_token;
    Poco::URI::decode(access_token, decoded_access_token);
    assert(decoded_access_token.size() <= std::numeric_limits<int32_t>::max()
           && uri.size() <= std::numeric_limits<int32_t>::max());
    const size_t size = 4 + decoded_access_token.size() + 4 + uri.size() + 4 + 8;
    Poco::Buffer<char> buffer(size); // allocate enough size
    buffer.resize(0); // start from empty buffer
    Poco::MemoryBinaryWriter writer(buffer, Poco::BinaryWriter::NETWORK_BYTE_ORDER);
    writer << static_cast<int32_t>(decoded_access_token.size())
           << decoded_access_token
           << static_cast<int32_t>(uri.size())
           << uri
           << int32_t(8)
           << ticks;
    assert(buffer.size() == size);
    return std::string(buffer.begin(), buffer.end());
}

static std::string ProofKeyPath()
{
    std::string keyPath
        = Poco::Path(Poco::Util::Application::instance().commandPath()).parent().toString()
          + "WOPI_proof_key";
    if (!Poco::File(keyPath).exists())
        keyPath = LOOLWSD::FileServerRoot + "/WOPI_proof_key";

    return keyPath;
}

static const Poco::Crypto::RSAKey* WopiKey()
{
    static const Poco::Crypto::RSAKey* const pKey = []() -> Poco::Crypto::RSAKey* {
        try
        {
            return new Poco::Crypto::RSAKey("", ProofKeyPath());
        }
        catch (const Poco::Exception& e)
        {
            LOG_ERR("Could not open WOPI proof RSA key: " << e.displayText());
        }
        catch (const std::exception& e)
        {
            LOG_ERR("Could not open WOPI proof RSA key: " << e.what());
        }
        catch (...)
        {
            LOG_ERR("Could not open WOPI proof RSA key: unknown exception");
        }
        return nullptr;
    }();
    return pKey;
}

std::string SignWopiProof(const std::string proof)
{
    std::ostringstream ostr;
    if (WopiKey())
    {
        static Poco::Crypto::RSADigestEngine digestEngine(*WopiKey(), "SHA256");
        digestEngine.update(proof.c_str(), proof.length());
        Poco::Crypto::DigestEngine::Digest digest = digestEngine.signature();
        // The signature generated contains CRLF line endings.
        // Use a line ending converter to remove these CRLF
        Poco::OutputLineEndingConverter lineEndingConv(ostr, "");
        Poco::Base64Encoder encoder(lineEndingConv);
        encoder << std::string(digest.begin(), digest.end());
        encoder.close();
    }
    return ostr.str();
}

std::string GetWopiProofKey()
{
    static const std::string publicKey = []{
        std::ostringstream oss;
        Poco::OutputLineEndingConverter lineEndingConv(oss, "");
        if (WopiKey()) {
            WopiKey()->save(&lineEndingConv);
        }
        std::string sKey = oss.str();
        const std::string sBegin = "-----BEGIN RSA PUBLIC KEY-----";
        const std::string sEnd = "-----END RSA PUBLIC KEY-----";
        auto pos = sKey.find(sBegin);
        if (pos != std::string::npos)
            sKey = sKey.substr(pos + sBegin.length());
        pos = sKey.find(sEnd);
        if (pos != std::string::npos)
            sKey = sKey.substr(0, pos);
        return sKey;
    }();
    return publicKey;
}

std::string GetWopiProofModulus()
{
    static const std::string modulus = []{
        std::ostringstream oss;
        if (WopiKey()) {
            const auto m = WopiKey()->modulus();
            Poco::Base64Encoder encoder(oss);
            encoder << std::string(m.begin(), m.end());
            encoder.close();
        }
        return oss.str();
    }();
    return modulus;
}

std::string GetWopiProofExponent()
{
    static const std::string exponent = []{
        std::ostringstream oss;
        if (WopiKey()) {
            const auto e = WopiKey()->decryptionExponent();
            Poco::Base64Encoder encoder(oss);
            encoder << std::string(e.begin(), e.end());
            encoder.close();
        }
        return oss.str();
    }();
    return exponent;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
