/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include <cassert>
#include <climits>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <Poco/DigestEngine.h>
#include <Poco/DirectoryIterator.h>
#include <Poco/Exception.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/StringTokenizer.h>
#include <Poco/Timestamp.h>
#include <Poco/URI.h>

#include "Storage.hpp"
#include "LOOLProtocol.hpp"
#include "TileCache.hpp"
#include "Util.hpp"
#include "MasterProcessSession.hpp"
#include "Unit.hpp"

using Poco::DirectoryIterator;
using Poco::File;
using Poco::FileException;
using Poco::Path;
using Poco::StringTokenizer;
using Poco::Timestamp;
using Poco::URI;

using namespace LOOLProtocol;

TileCache::TileCache(const std::string& docURL,
                     const Timestamp& modifiedTime,
                     const std::string& cacheDir) :
    _docURL(docURL),
    _cacheDir(cacheDir)
{
    Log::info() << "TileCache ctor for uri [" << _docURL
                << "] modifiedTime=" << (modifiedTime.raw()/1000000)
                << " getLastModified()=" << (getLastModified().raw()/1000000) << Log::end;
    File directory(_cacheDir);
    if (directory.exists() &&
        (getLastModified() < modifiedTime ||
         getTextFile("unsaved.txt") != ""))
    {
        // Document changed externally or modifications were not saved after all. Cache not useful.
        Util::removeFile(_cacheDir, true);
        Log::info("Completely cleared tile cache: " + _cacheDir);
    }

    File(_cacheDir).createDirectories();

    saveLastModified(modifiedTime);
}

TileCache::~TileCache()
{
    Log::info("~TileCache dtor for uri [" + _docURL + "].");
}

struct TileCache::TileBeingRendered
{
    Poco::Timestamp _startTime;
    std::vector<std::weak_ptr<MasterProcessSession>> _subscribers;
    TileBeingRendered()
    {
        _startTime.update();
    }
};

std::shared_ptr<TileCache::TileBeingRendered> TileCache::findTileBeingRendered(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight)
{
    const std::string cachedName = cacheFileName(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);

    Util::assertIsLocked(_tilesBeingRenderedMutex);

    const auto tile = _tilesBeingRendered.find(cachedName);
    return (tile != _tilesBeingRendered.end() ? tile->second : nullptr);
}

void TileCache::forgetTileBeingRendered(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight)
{
    const std::string cachedName = cacheFileName(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);

    Util::assertIsLocked(_tilesBeingRenderedMutex);

    assert(_tilesBeingRendered.find(cachedName) != _tilesBeingRendered.end());
    _tilesBeingRendered.erase(cachedName);
}

std::unique_ptr<std::fstream> TileCache::lookupTile(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight)
{
    const std::string fileName = _cacheDir + "/" + cacheFileName(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);

    std::unique_ptr<std::fstream> result(new std::fstream(fileName, std::ios::in));
    UnitWSD::get().lookupTile(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight, result);

    if (result && result->is_open())
    {
        Log::trace("Found cache tile: " + fileName);
        return result;
    }

    return nullptr;
}

void TileCache::saveTile(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight, const char *data, size_t size)
{
    const std::string fileName = _cacheDir + "/" + cacheFileName(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);

    Log::trace() << "Saving cache tile: " << fileName << Log::end;

    std::fstream outStream(fileName, std::ios::out);
    outStream.write(data, size);
    outStream.close();
}

std::string TileCache::getTextFile(const std::string& fileName)
{
    const std::string fullFileName =  _cacheDir + "/" + fileName;

    std::fstream textStream(fullFileName, std::ios::in);
    if (!textStream.is_open())
    {
        Log::info("Could not open " + fullFileName);
        return "";
    }

    std::vector<char> buffer;
    textStream.seekg(0, std::ios_base::end);
    std::streamsize size = textStream.tellg();
    buffer.resize(size);
    textStream.seekg(0, std::ios_base::beg);
    textStream.read(buffer.data(), size);
    textStream.close();

    if (buffer.size() > 0 && buffer.back() == '\n')
        buffer.pop_back();

    std::string result = std::string(buffer.data(), buffer.size());
    Log::info("Read '" + result + "' from " + fullFileName);

    return result;
}

void TileCache::saveTextFile(const std::string& text, const std::string& fileName)
{
    const std::string fullFileName = _cacheDir + "/" + fileName;
    std::fstream textStream(fullFileName, std::ios::out);

    if (!textStream.is_open())
    {
        Log::error("Could not save '" + text + "' to " + fullFileName);
        return;
    }
    else
    {
        Log::info("Saving '" + text + "' to " + fullFileName);
    }

    textStream << text << std::endl;
    textStream.close();
}

void TileCache::setUnsavedChanges(bool state)
{
    if (state)
        saveTextFile("1", "unsaved.txt");
    else
        removeFile("unsaved.txt");
}

void TileCache::saveRendering(const std::string& name, const std::string& dir, const char *data, size_t size)
{
    // can fonts be invalidated?
    const std::string dirName = _cacheDir + "/" + dir;

    File(dirName).createDirectories();

    const std::string fileName = dirName + "/" + name;

    std::fstream outStream(fileName, std::ios::out);
    outStream.write(data, size);
    outStream.close();
}

std::unique_ptr<std::fstream> TileCache::lookupRendering(const std::string& name, const std::string& dir)
{
    const std::string dirName = _cacheDir + "/" + dir;
    const std::string fileName = dirName + "/" + name;
    File directory(dirName);

    if (directory.exists() && directory.isDirectory() && File(fileName).exists())
    {
        std::unique_ptr<std::fstream> result(new std::fstream(fileName, std::ios::in));
        return result;
    }

    return nullptr;
}

void TileCache::invalidateTiles(int part, int x, int y, int width, int height)
{
    Log::trace() << "Removing invalidated tiles: part: " << part
                 << ", x: " << x << ", y: " << y
                 << ", width: " << width
                 << ", height: " << height << Log::end;

    File dir(_cacheDir);
    if (dir.exists() && dir.isDirectory())
    {
        std::unique_lock<std::mutex> lock(_cacheMutex);
        for (auto tileIterator = DirectoryIterator(dir); tileIterator != DirectoryIterator(); ++tileIterator)
        {
            const std::string fileName = tileIterator.path().getFileName();
            if (intersectsTile(fileName, part, x, y, width, height))
            {
                Log::debug("Removing tile: " + tileIterator.path().toString());
                Util::removeFile(tileIterator.path());
            }
        }
    }
}

void TileCache::invalidateTiles(const std::string& tiles)
{
    StringTokenizer tokens(tiles, " ", StringTokenizer::TOK_IGNORE_EMPTY | StringTokenizer::TOK_TRIM);

    assert(tokens[0] == "invalidatetiles:");

    if (tokens.count() == 2 && tokens[1] == "EMPTY")
    {
        invalidateTiles(-1, 0, 0, INT_MAX, INT_MAX);
    }
    else if (tokens.count() != 6)
    {
        return;
    }
    else
    {
        int part, x, y, width, height;
        if (getTokenInteger(tokens[1], "part", part) &&
            getTokenInteger(tokens[2], "x", x) &&
            getTokenInteger(tokens[3], "y", y) &&
            getTokenInteger(tokens[4], "width", width) &&
            getTokenInteger(tokens[5], "height", height))
        {
            invalidateTiles(part, x, y, width, height);
        }
    }
}

void TileCache::removeFile(const std::string& fileName)
{
    const std::string fullFileName = _cacheDir + "/" + fileName;

    if (std::remove(fullFileName.c_str()) == 0)
        Log::info("Removed file: " + fullFileName);
}

std::string TileCache::cacheFileName(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight)
{
    std::ostringstream oss;
    oss << part << '_' << width << 'x' << height << '.'
        << tilePosX << ',' << tilePosY << '.'
        << tileWidth << 'x' << tileHeight << ".png";
    return oss.str();
}

bool TileCache::parseCacheFileName(const std::string& fileName, int& part, int& width, int& height, int& tilePosX, int& tilePosY, int& tileWidth, int& tileHeight)
{
    return (std::sscanf(fileName.c_str(), "%d_%dx%d.%d,%d.%dx%d.png", &part, &width, &height, &tilePosX, &tilePosY, &tileWidth, &tileHeight) == 7);
}

bool TileCache::intersectsTile(const std::string& fileName, int part, int x, int y, int width, int height)
{
    int tilePart, tilePixelWidth, tilePixelHeight, tilePosX, tilePosY, tileWidth, tileHeight;
    if (parseCacheFileName(fileName, tilePart, tilePixelWidth, tilePixelHeight, tilePosX, tilePosY, tileWidth, tileHeight))
    {
        if (part != -1 && tilePart != part)
            return false;

        const int left = std::max(x, tilePosX);
        const int right = std::min(x + width, tilePosX + tileWidth);
        const int top = std::max(y, tilePosY);
        const int bottom = std::min(y + height, tilePosY + tileHeight);

        if (left <= right && top <= bottom)
            return true;
    }

    return false;
}

Timestamp TileCache::getLastModified()
{
    std::fstream modTimeFile(_cacheDir + "/modtime.txt", std::ios::in);

    if (!modTimeFile.is_open())
        return 0;

    Timestamp::TimeVal result;
    modTimeFile >> result;

    modTimeFile.close();
    return result;
}

void TileCache::saveLastModified(const Timestamp& timestamp)
{
    std::fstream modTimeFile(_cacheDir + "/modtime.txt", std::ios::out);
    modTimeFile << timestamp.raw() << std::endl;
    modTimeFile.close();
}

void TileCache::notifyAndRemoveSubscribers(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight, MasterProcessSession *emitter)
{
    std::unique_lock<std::mutex> lock(_tilesBeingRenderedMutex);

    std::shared_ptr<TileBeingRendered> tileBeingRendered = findTileBeingRendered(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);
    if (!tileBeingRendered)
        return;

    Log::debug("Sending tile message also to subscribers");

    for (const auto& i: tileBeingRendered->_subscribers)
    {
        auto subscriber = i.lock();
        if (subscriber)
        {
            if (subscriber.get() == emitter)
            {
                Log::error("Refusing to queue new tile message for ourselves");
                continue;
            }

            std::shared_ptr<BasicTileQueue> queue;
            queue = subscriber->getQueue();
            // Re-emit the tile command in the other thread(s) to re-check and hit
            // the cache. Construct the message from scratch to contain only the
            // mandatory parts of the message.
            if (queue)
            {
                const std::string message("tile "
                                          " part=" + std::to_string(part) +
                                          " width=" + std::to_string(width) +
                                          " height=" + std::to_string(height) +
                                          " tileposx=" + std::to_string(tilePosX) +
                                          " tileposy=" + std::to_string(tilePosY) +
                                          " tilewidth=" + std::to_string(tileWidth) +
                                          " tileheight=" + std::to_string(tileHeight) +
                                          "\n");
                queue->put(message);
            }
        }
    }
    forgetTileBeingRendered(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);
}

// FIXME: to be further simplified when we centralize tile messages.
bool TileCache::isTileBeingRenderedIfSoSubscribe(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight, const std::shared_ptr<MasterProcessSession> &subscriber)
{
    std::unique_lock<std::mutex> lock(_tilesBeingRenderedMutex);

    std::shared_ptr<TileBeingRendered> tileBeingRendered = findTileBeingRendered(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);

    if (tileBeingRendered)
    {
        Log::debug("Tile is already being rendered, subscribing");
        assert(subscriber->getKind() == LOOLSession::Kind::ToClient);

        for (const auto &s : tileBeingRendered->_subscribers)
        {
            if (s.lock().get() == subscriber.get())
            {
                Log::debug("Redundant request to re-subscribe on a tile");
                return true;
            }
        }
        tileBeingRendered->_subscribers.push_back(subscriber);

        return true;
    }
    else
    {
        const std::string cachedName = cacheFileName(part, width, height, tilePosX, tilePosY, tileWidth, tileHeight);

        assert(_tilesBeingRendered.find(cachedName) == _tilesBeingRendered.end());

        _tilesBeingRendered[cachedName] = std::make_shared<TileBeingRendered>();

        return false;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
