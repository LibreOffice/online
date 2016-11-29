/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include <cppunit/extensions/HelperMacros.h>

#include "Common.hpp"
#include "Protocol.hpp"
#include "MessageQueue.hpp"
#include "Util.hpp"

namespace CPPUNIT_NS
{
template<>
struct assertion_traits<std::vector<char>>
{
    static bool equal(const std::vector<char>& x, const std::vector<char>& y)
    {
        return x == y;
    }

    static std::string toString(const std::vector<char>& x)
    {
        const std::string text = '"' + (!x.empty() ? std::string(x.data(), x.size()) : "<empty>") + '"';
        return text;
    }
};
}

/// TileQueue unit-tests.
class TileQueueTests : public CPPUNIT_NS::TestFixture
{
    CPPUNIT_TEST_SUITE(TileQueueTests);

    CPPUNIT_TEST(testTileQueuePriority);
    CPPUNIT_TEST(testTileCombinedRendering);
    CPPUNIT_TEST(testTileRecombining);
    CPPUNIT_TEST(testViewOrder);
    CPPUNIT_TEST(testPreviewsDeprioritization);

    CPPUNIT_TEST_SUITE_END();

    void testTileQueuePriority();
    void testTileCombinedRendering();
    void testTileRecombining();
    void testViewOrder();
    void testPreviewsDeprioritization();
};

void TileQueueTests::testTileQueuePriority()
{
    const std::string reqHigh = "tile part=0 width=256 height=256 tileposx=0 tileposy=0 tilewidth=3840 tileheight=3840";
    const std::string resHigh = "tile part=0 width=256 height=256 tileposx=0 tileposy=0 tilewidth=3840 tileheight=3840 ver=-1";
    const TileQueue::Payload payloadHigh(resHigh.data(), resHigh.data() + resHigh.size());
    const std::string reqLow = "tile part=0 width=256 height=256 tileposx=0 tileposy=253440 tilewidth=3840 tileheight=3840";
    const std::string resLow = "tile part=0 width=256 height=256 tileposx=0 tileposy=253440 tilewidth=3840 tileheight=3840 ver=-1";
    const TileQueue::Payload payloadLow(resLow.data(), resLow.data() + resLow.size());

    TileQueue queue;

    // Request the tiles.
    queue.put(reqLow);
    queue.put(reqHigh);

    // Original order.
    CPPUNIT_ASSERT_EQUAL(payloadLow, queue.get());
    CPPUNIT_ASSERT_EQUAL(payloadHigh, queue.get());

    // Request the tiles.
    queue.put(reqLow);
    queue.put(reqHigh);
    queue.put(reqHigh);
    queue.put(reqLow);

    // Set cursor above reqHigh.
    queue.updateCursorPosition(0, 0, 0, 0, 10, 100);

    // Prioritized order.
    CPPUNIT_ASSERT_EQUAL(payloadHigh, queue.get());
    CPPUNIT_ASSERT_EQUAL(payloadLow, queue.get());

    // Repeat with cursor position set.
    queue.put(reqLow);
    queue.put(reqHigh);
    CPPUNIT_ASSERT_EQUAL(payloadHigh, queue.get());
    CPPUNIT_ASSERT_EQUAL(payloadLow, queue.get());

    // Repeat by changing cursor position.
    queue.put(reqLow);
    queue.put(reqHigh);
    queue.updateCursorPosition(0, 0, 0, 253450, 10, 100);
    CPPUNIT_ASSERT_EQUAL(payloadLow, queue.get());
    CPPUNIT_ASSERT_EQUAL(payloadHigh, queue.get());
}

void TileQueueTests::testTileCombinedRendering()
{
    const std::string req1 = "tile part=0 width=256 height=256 tileposx=0 tileposy=0 tilewidth=3840 tileheight=3840";
    const std::string req2 = "tile part=0 width=256 height=256 tileposx=3840 tileposy=0 tilewidth=3840 tileheight=3840";
    const std::string req3 = "tile part=0 width=256 height=256 tileposx=0 tileposy=3840 tilewidth=3840 tileheight=3840";
    const std::string req4 = "tile part=0 width=256 height=256 tileposx=3840 tileposy=3840 tilewidth=3840 tileheight=3840";

    const std::string resHor = "tilecombine part=0 width=256 height=256 tileposx=0,3840 tileposy=0,0 imgsize=0,0 tilewidth=3840 tileheight=3840";
    const TileQueue::Payload payloadHor(resHor.data(), resHor.data() + resHor.size());
    const std::string resVer = "tilecombine part=0 width=256 height=256 tileposx=0,0 tileposy=0,3840 imgsize=0,0 tilewidth=3840 tileheight=3840";
    const TileQueue::Payload payloadVer(resVer.data(), resVer.data() + resVer.size());
    const std::string resFull = "tilecombine part=0 width=256 height=256 tileposx=0,3840,0 tileposy=0,0,3840 imgsize=0,0,0 tilewidth=3840 tileheight=3840";
    const TileQueue::Payload payloadFull(resFull.data(), resFull.data() + resFull.size());

    TileQueue queue;

    // Horizontal.
    queue.put(req1);
    queue.put(req2);
    CPPUNIT_ASSERT_EQUAL(payloadHor, queue.get());

    // Vertical.
    queue.put(req1);
    queue.put(req3);
    CPPUNIT_ASSERT_EQUAL(payloadVer, queue.get());

    // Vertical.
    queue.put(req1);
    queue.put(req2);
    queue.put(req3);
    CPPUNIT_ASSERT_EQUAL(payloadFull, queue.get());
}

namespace {

std::string payloadAsString(const MessageQueue::Payload& payload)
{
    return std::string(payload.data(), payload.size());
}

}

void TileQueueTests::testTileRecombining()
{
    TileQueue queue;

    queue.put("tilecombine part=0 width=256 height=256 tileposx=0,3840,7680 tileposy=0,0,0 tilewidth=3840 tileheight=3840");
    queue.put("tilecombine part=0 width=256 height=256 tileposx=0,3840 tileposy=0,0 tilewidth=3840 tileheight=3840");

    // the tilecombine's get merged, resulting in 3 "tile" messages
    CPPUNIT_ASSERT_EQUAL(3, static_cast<int>(queue._queue.size()));

    // but when we later extract that, it is just one "tilecombine" message
    std::string message(payloadAsString(queue.get()));

    CPPUNIT_ASSERT_EQUAL(std::string("tilecombine part=0 width=256 height=256 tileposx=7680,0,3840 tileposy=0,0,0 imgsize=0,0,0 tilewidth=3840 tileheight=3840"), message);

    // and nothing remains in the queue
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(queue._queue.size()));
}

void TileQueueTests::testViewOrder()
{
    TileQueue queue;

    // should result in the 3, 2, 1, 0 order of the views
    queue.updateCursorPosition(0, 0, 0, 0, 10, 100);
    queue.updateCursorPosition(2, 0, 0, 0, 10, 100);
    queue.updateCursorPosition(1, 0, 0, 7680, 10, 100);
    queue.updateCursorPosition(3, 0, 0, 0, 10, 100);
    queue.updateCursorPosition(2, 0, 0, 15360, 10, 100);
    queue.updateCursorPosition(3, 0, 0, 23040, 10, 100);

    const std::vector<std::string> tiles =
    {
        "tile part=0 width=256 height=256 tileposx=0 tileposy=0 tilewidth=3840 tileheight=3840 ver=-1",
        "tile part=0 width=256 height=256 tileposx=0 tileposy=7680 tilewidth=3840 tileheight=3840 ver=-1",
        "tile part=0 width=256 height=256 tileposx=0 tileposy=15360 tilewidth=3840 tileheight=3840 ver=-1",
        "tile part=0 width=256 height=256 tileposx=0 tileposy=23040 tilewidth=3840 tileheight=3840 ver=-1"
    };

    for (auto &tile : tiles)
        queue.put(tile);

    CPPUNIT_ASSERT_EQUAL(4, static_cast<int>(queue._queue.size()));

    // should result in the 3, 2, 1, 0 order of the tiles thanks to the cursor
    // positions
    for (size_t i = 0; i < tiles.size(); ++i)
    {
        CPPUNIT_ASSERT_EQUAL(tiles[3 - i], payloadAsString(queue.get()));
    }
}

void TileQueueTests::testPreviewsDeprioritization()
{
    TileQueue queue;

    // simple case - put previews to the queue and get everything back again
    const std::vector<std::string> previews =
    {
        "tile part=0 width=180 height=135 tileposx=0 tileposy=0 tilewidth=15875 tileheight=11906 ver=-1 id=0",
        "tile part=1 width=180 height=135 tileposx=0 tileposy=0 tilewidth=15875 tileheight=11906 ver=-1 id=1",
        "tile part=2 width=180 height=135 tileposx=0 tileposy=0 tilewidth=15875 tileheight=11906 ver=-1 id=2",
        "tile part=3 width=180 height=135 tileposx=0 tileposy=0 tilewidth=15875 tileheight=11906 ver=-1 id=3"
    };

    for (auto &preview : previews)
        queue.put(preview);

    for (size_t i = 0; i < previews.size(); ++i)
    {
        CPPUNIT_ASSERT_EQUAL(previews[i], payloadAsString(queue.get()));
    }

    // stays empty after all is done
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(queue._queue.size()));

    // re-ordering case - put previews and normal tiles to the queue and get
    // everything back again but this time the tiles have to interleave with
    // the previews
    const std::vector<std::string> tiles =
    {
        "tile part=0 width=256 height=256 tileposx=0 tileposy=0 tilewidth=3840 tileheight=3840 ver=-1",
        "tile part=0 width=256 height=256 tileposx=0 tileposy=7680 tilewidth=3840 tileheight=3840 ver=-1"
    };

    for (auto &preview : previews)
        queue.put(preview);

    queue.put(tiles[0]);

    CPPUNIT_ASSERT_EQUAL(previews[0], payloadAsString(queue.get()));
    CPPUNIT_ASSERT_EQUAL(tiles[0], payloadAsString(queue.get()));
    CPPUNIT_ASSERT_EQUAL(previews[1], payloadAsString(queue.get()));

    queue.put(tiles[1]);

    CPPUNIT_ASSERT_EQUAL(previews[2], payloadAsString(queue.get()));
    CPPUNIT_ASSERT_EQUAL(tiles[1], payloadAsString(queue.get()));
    CPPUNIT_ASSERT_EQUAL(previews[3], payloadAsString(queue.get()));

    // stays empty after all is done
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(queue._queue.size()));

    // cursor positioning case - the cursor position should not prioritize the
    // previews
    queue.updateCursorPosition(0, 0, 0, 0, 10, 100);

    queue.put(tiles[1]);
    queue.put(previews[0]);

    CPPUNIT_ASSERT_EQUAL(tiles[1], payloadAsString(queue.get()));
    CPPUNIT_ASSERT_EQUAL(previews[0], payloadAsString(queue.get()));

    // stays empty after all is done
    CPPUNIT_ASSERT_EQUAL(0, static_cast<int>(queue._queue.size()));
}

CPPUNIT_TEST_SUITE_REGISTRATION(TileQueueTests);

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
