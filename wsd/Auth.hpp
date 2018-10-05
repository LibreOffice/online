/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Authentication and Authorization support.
#ifndef INCLUDED_AUTH_HPP
#define INCLUDED_AUTH_HPP

#include <cassert>
#include <string>

#include <Poco/Crypto/RSADigestEngine.h>
#include <Poco/Crypto/RSAKey.h>
#include <Poco/Net/HTTPRequest.h>
#include <Poco/URI.h>

/// Class to keep the authorization data.
class Authorization
{
public:
    enum class Type
    {
        None,
        Token,
        Header
    };

private:
    Type _type;
    std::string _data;

public:
    Authorization()
        : _type(Type::None)
    {
    }

    Authorization(Type type, const std::string& data)
        : _type(type)
        , _data(data)
    {
    }

    /// Set the access_token parametr to the given uri.
    void authorizeURI(Poco::URI& uri) const;

    /// Set the Authorization: header in request.
    void authorizeRequest(Poco::Net::HTTPRequest& request) const;
};

/// Base class of all Authentication/Authorization implementations.
class AuthBase
{
public:
    virtual ~AuthBase() {}
    /// Called to acquire an access token.
    virtual const std::string getAccessToken() = 0;

    /// Used to verify the validity of an access token.
    virtual bool verify(const std::string& token) = 0;
};

/// JWT Authorization.
class JWTAuth : public AuthBase
{
public:
    JWTAuth(const std::string& name, const std::string& sub, const std::string& aud)
        : _name(name),
          _sub(sub),
          _aud(aud),
          _digestEngine(_key, "SHA256")
    {
    }

    const std::string getAccessToken() override;

    bool verify(const std::string& accessToken) override;

private:
    const std::string createHeader();

    const std::string createPayload();

private:
    const std::string _alg = "RS256";
    const std::string _typ = "JWT";

    const std::string _iss = "lool";
    const std::string _name;
    const std::string _sub;
    const std::string _aud;

    static const Poco::Crypto::RSAKey _key;
    Poco::Crypto::RSADigestEngine _digestEngine;
};

/// OAuth Authorization.
class OAuth : public AuthBase
{
public:
    OAuth(const std::string& clientId,
          const std::string& clientSecret,
          const std::string& tokenEndPoint,
          const std::string& authVerifyUrl,
          const std::string& authorizationCode) :
        _clientId(clientId),
        _clientSecret(clientSecret),
        _tokenEndPoint(tokenEndPoint),
        _authVerifyUrl(authVerifyUrl),
        _authorizationCode(authorizationCode)
    {
    }

    const std::string getAccessToken() override;

    bool verify(const std::string& token) override;

private:
    const std::string _clientId;
    const std::string _clientSecret;
    const std::string _tokenEndPoint;
    const std::string _authVerifyUrl;
    const std::string _authorizationCode;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
