/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_TILEDESC_HPP
#define INCLUDED_TILEDESC_HPP

#include <map>
#include <sstream>
#include <string>

#include <Poco/StringTokenizer.h>

#include "Exceptions.hpp"
#include "LOOLProtocol.hpp"

/// Tile Descriptor
/// Represents a tile's coordinates and dimensions.
class TileDesc
{
public:
    TileDesc(int part, int width, int height, int tilePosX, int tilePosY, int tileWidth, int tileHeight, int id = -1) :
        _part(part),
        _width(width),
        _height(height),
        _tilePosX(tilePosX),
        _tilePosY(tilePosY),
        _tileWidth(tileWidth),
        _tileHeight(tileHeight),
        _id(id)
    {
        if (_part < 0 ||
            _width <= 0 ||
            _height <= 0 ||
            _tilePosX < 0 ||
            _tilePosY < 0 ||
            _tileWidth <= 0 ||
            _tileHeight <= 0)
        {
            throw BadArgumentException("Invalid tile descriptor.");
        }
    }

    int getPart() const { return _part; }
    int getWidth() const { return _width; }
    int getHeight() const { return _height; }
    int getTilePosX() const { return _tilePosX; }
    int getTilePosY() const { return _tilePosY; }
    int getTileWidth() const { return _tileWidth; }
    int getTileHeight() const { return _tileHeight; }

    /// Serialize this instance into a string.
    /// Optionally prepend a prefix.
    std::string serialize(const std::string& prefix = "") const
    {
        std::ostringstream oss;
        oss << prefix
            << " part=" << _part
            << " width=" << _width
            << " height=" << _height
            << " tileposx=" << _tilePosX
            << " tileposy=" << _tilePosY
            << " tilewidth=" << _tileWidth
            << " tileheight=" << _tileHeight;
        if (_id >= 0)
        {
            oss << " id=" << _id;
        }

        return oss.str();
    }

    /// Deserialize a TileDesc from a tokenized string.
    static
    TileDesc parse(const Poco::StringTokenizer& tokens)
    {
        // We don't expect undocument fields and
        // assume all values to be int.
        std::map<std::string, int> pairs;

        // id is optional.
        pairs["id"] = -1;

        for (size_t i = 0; i < tokens.count(); ++i)
        {
            std::string name;
            int value = -1;
            if (LOOLProtocol::parseNameIntegerPair(tokens[i], name, value))
            {
                pairs[name] = value;
            }
        }

        return TileDesc(pairs["part"], pairs["width"], pairs["height"],
                        pairs["tileposx"], pairs["tileposy"],
                        pairs["tilewidth"], pairs["tileheight"],
                        pairs["id"]);
    }

    /// Deserialize a TileDesc from a string format.
    static
    TileDesc parse(const std::string& message)
    {
        Poco::StringTokenizer tokens(message, " ",
                                     Poco::StringTokenizer::TOK_IGNORE_EMPTY |
                                     Poco::StringTokenizer::TOK_TRIM);
        return parse(tokens);
    }

private:
    int _part;
    int _width;
    int _height;
    int _tilePosX;
    int _tilePosY;
    int _tileWidth;
    int _tileHeight;
    int _id;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
