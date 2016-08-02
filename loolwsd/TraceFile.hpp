/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

/// Dumps commands and notification trace.
class TraceFileWriter
{
public:
    TraceFileWriter(const std::string& path) :
        _epochStart(Poco::Timestamp().epochMicroseconds()),
        _stream(path, std::ios::out)
    {
    }

    ~TraceFileWriter()
    {
        _stream.close();
    }

    void writeIncoming(const std::string& data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        const Poco::Int64 usec = Poco::Timestamp().epochMicroseconds() - _epochStart;
        _stream.write(">", 1);
        _stream << usec;
        _stream.write(">", 1);
        _stream.write(data.c_str(), data.size());
        _stream.write("\n", 1);
    }

    void writeOutgoing(const std::string& data)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        const Poco::Int64 usec = Poco::Timestamp().epochMicroseconds() - _epochStart;
        _stream.write("<", 1);
        _stream << usec;
        _stream.write("<", 1);
        _stream.write(data.c_str(), data.size());
        _stream.write("\n", 1);
    }

private:
    const Poco::Int64 _epochStart;
    std::fstream _stream;
    std::mutex _mutex;
};

class TraceFileRecord
{
public:
    enum class Direction
    {
        Invalid,
        Incoming,
        Outgoing
    };

    TraceFileRecord() :
        Dir(Direction::Invalid)
    {
    }

    Direction Dir;
    unsigned TimestampNs;
    std::string Payload;
};

class TraceFileReader
{
public:
    TraceFileReader(const std::string& path) :
        _epochStart(Poco::Timestamp().epochMicroseconds()),
        _stream(path),
        _indexIn(-1),
        _indexOut(-1)
    {
        readFile();
    }

    const std::string& getDocURI() const { return _docURI; }

    TraceFileRecord getNextRecord(const TraceFileRecord::Direction dir)
    {
        if (dir == TraceFileRecord::Direction::Incoming)
        {
            if (_indexIn < _records.size())
            {
                const TraceFileRecord rec = _records[_indexIn];
                _indexIn = advance(_indexIn, dir);
                return rec;
            }
        }
        else
        {
            if (_indexOut < _records.size())
            {
                const TraceFileRecord rec = _records[_indexOut];
                _indexOut = advance(_indexOut, dir);
                return rec;
            }
        }

        // Invalid.
        return TraceFileRecord();
    }

private:
    void readFile()
    {
        _records.clear();

        std::string line;
        while (std::getline(_stream, line) && !line.empty())
        {
            const auto v = split(line, line[0]);
            if (v.size() == 2)
            {
                TraceFileRecord rec;
                rec.Dir = (line[0] == '>' ? TraceFileRecord::Direction::Incoming : TraceFileRecord::Direction::Outgoing);
                rec.TimestampNs = std::atoi(v[0].c_str());
                rec.Payload = v[1];
                _records.push_back(rec);
            }
        }

        _indexIn = advance(-1, TraceFileRecord::Direction::Incoming);
        _indexOut = advance(-1, TraceFileRecord::Direction::Outgoing);

        if (_records.size() > 1)
        {
            if (_records[0].Payload.find("loolclient") == 0 &&
                _records[1].Payload.find("load url=") == 0)
            {
                _docURI = _records[1].Payload.substr(9);
                return;
            }
            else if (_records[0].Payload.find("load url=") == 0)
            {
                _docURI = _records[0].Payload.substr(9);
                return;
            }
        }

        fprintf(stderr, "Invalid trace file with %ld records. First record: %s\n", _records.size(),
                _records.empty() ? "<empty>" : _records[0].Payload.c_str());
        throw std::runtime_error("Invalid trace file.");
    }

    std::vector<std::string> split(const std::string& s, const char delim) const
    {
        std::stringstream ss(s);
        std::string item;
        std::vector<std::string> v;
        while (std::getline(ss, item, delim))
        {
            if (!item.empty())
            {
                v.push_back(item);
            }
        }

        return v;
    }

    unsigned advance(unsigned index, const TraceFileRecord::Direction dir)
    {
        while (++index < _records.size())
        {
            if (_records[index].Dir == dir)
            {
                break;
            }
        }

        return index;
    }

private:
    const Poco::Int64 _epochStart;
    std::ifstream _stream;
    std::vector<TraceFileRecord> _records;
    std::string _docURI;
    unsigned _indexIn;
    unsigned _indexOut;
};

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
