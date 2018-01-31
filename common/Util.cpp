/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "config.h"

#include "Util.hpp"

#include <execinfo.h>
#include <csignal>
#include <sys/poll.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include <Poco/Base64Encoder.h>
#include <Poco/ConsoleChannel.h>
#include <Poco/Exception.h>
#include <Poco/Format.h>
#include <Poco/JSON/JSON.h>
#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>
#include <Poco/Net/WebSocket.h>
#include <Poco/Process.h>
#include <Poco/RandomStream.h>
#include <Poco/TemporaryFile.h>
#include <Poco/Thread.h>
#include <Poco/Timestamp.h>
#include <Poco/Util/Application.h>

#include "Common.hpp"
#include "Log.hpp"
#include "Util.hpp"

namespace Util
{
    namespace rng
    {
        static std::random_device _rd;
        static std::mutex _rngMutex;
        static Poco::RandomBuf _randBuf;

        // Create the prng with a random-device for seed.
        // If we don't have a hardware random-device, we will get the same seed.
        // In that case we are better off with an arbitrary, but changing, seed.
        static std::mt19937_64 _rng = std::mt19937_64(_rd.entropy()
                                                    ? _rd()
                                                    : (clock() + getpid()));

        // A new seed is used to shuffle the sequence.
        // N.B. Always reseed after getting forked!
        void reseed()
        {
            _rng.seed(_rd.entropy() ? _rd() : (clock() + getpid()));
        }

        // Returns a new random number.
        unsigned getNext()
        {
            std::unique_lock<std::mutex> lock(_rngMutex);
            return _rng();
        }

        std::vector<char> getBytes(const size_t length)
        {
            std::vector<char> v(length);
            _randBuf.readFromDevice(v.data(), v.size());
            return v;
        }

        /// Generates a random string in Base64.
        /// Note: May contain '/' characters.
        std::string getB64String(const size_t length)
        {
            std::stringstream ss;
            Poco::Base64Encoder b64(ss);
            b64.write(getBytes(length).data(), length);
            return ss.str().substr(0, length);
        }

        std::string getFilename(const size_t length)
        {
            std::string s = getB64String(length * 2);
            s.erase(std::remove_if(s.begin(), s.end(),
                                   [](const std::string::value_type& c)
                                   {
                                       // Remove undesirable characters in a filename.
                                       return c == '/' || c == ' ' || c == '+';
                                   }),
                     s.end());
            return s.substr(0, length);
        }
    }

    // close what we have - far faster than going up to a 1m open_max eg.
    static bool closeFdsFromProc()
    {
          DIR *fdDir = opendir("/proc/self/fd");
          if (!fdDir)
              return false;

          struct dirent *i;

          while ((i = readdir(fdDir))) {
              if (i->d_name[0] == '.')
                  continue;

              char *e = NULL;
              errno = 0;
              long fd = strtol(i->d_name, &e, 10);
              if (errno != 0 || !e || *e)
                  continue;

              if (fd == dirfd(fdDir))
                  continue;

              if (fd < 3)
                  continue;

              if (close(fd) < 0)
                  std::cerr << "Unexpected failure to close fd " << fd << std::endl;
          }

          closedir(fdDir);
          return true;
    }

    static void closeFds()
    {
        if (!closeFdsFromProc())
        {
            LOG_WRN("Couldn't close fds efficiently from /proc");
            for (int fd = 3; fd < sysconf(_SC_OPEN_MAX); ++fd)
                close(fd);
        }
    }

    int spawnProcess(const std::string &cmd, const std::vector<std::string> &args, int *stdInput)
    {
        int pipeFds[2] = { -1, -1 };
        if (stdInput)
        {
            if (pipe(pipeFds) < 0)
            {
                LOG_ERR("Out of file descriptors spawning " << cmd);
                throw Poco::SystemException("Out of file descriptors");
            }
        }

        std::vector<char *> params;
        params.push_back(const_cast<char *>(cmd.c_str()));
        for (auto i : args)
            params.push_back(const_cast<char *>(i.c_str()));
        params.push_back(nullptr);

        int pid = fork();
        if (pid < 0)
        {
            LOG_ERR("Failed to fork for command '" << cmd);
            throw Poco::SystemException("Failed to fork for command ", cmd);
        }
        else if (pid == 0) // child
        {
            if (stdInput)
                dup2(pipeFds[0], STDIN_FILENO);

            closeFds();

            int ret = execvp(params[0], &params[0]);
            if (ret < 0)
                std::cerr << "Failed to exec command '" << cmd << "' with error '" << strerror(errno) << "'\n";
            _exit(42);
        }
        // else spawning process still
        if (stdInput)
        {
            close(pipeFds[0]);
            *stdInput = pipeFds[1];
        }
        return pid;
    }

    bool dataFromHexString(const std::string& hexString, std::vector<unsigned char>& data)
    {
        if (hexString.length() % 2 != 0)
        {
            return false;
        }

        data.clear();
        std::stringstream stream;
        unsigned value;
        for (unsigned long offset = 0; offset < hexString.size(); offset += 2)
        {
            stream.clear();
            stream << std::hex << hexString.substr(offset, 2);
            stream >> value;
            data.push_back(static_cast<unsigned char>(value));
        }

        return true;
    }

    std::string encodeId(const unsigned number, const int padding)
    {
        std::ostringstream oss;
        oss << std::hex << std::setw(padding) << std::setfill('0') << number;
        return oss.str();
    }

    unsigned decodeId(const std::string& str)
    {
        unsigned id = 0;
        std::stringstream ss;
        ss << std::hex << str;
        ss >> id;
        return id;
    }

    bool windowingAvailable()
    {
        return std::getenv("DISPLAY") != nullptr;
    }

    static const char *startsWith(const char *line, const char *tag)
    {
        int len = strlen(tag);
        if (!strncmp(line, tag, len))
        {
            while (!isdigit(line[len]) && line[len] != '\0')
                ++len;

            return line + len;
        }

        return nullptr;
    }

    size_t getTotalSystemMemory()
    {
        size_t totalMemKb = 0;
        FILE* file = fopen("/proc/meminfo", "r");
        if (file != nullptr)
        {
            char line[4096] = { 0 };
            while (fgets(line, sizeof(line), file))
            {
                const char* value;
                if ((value = startsWith(line, "MemTotal:")))
                {
                    totalMemKb = atoi(value);
                    break;
                }
            }
        }

        return totalMemKb;
    }

    std::pair<size_t, size_t> getPssAndDirtyFromSMaps(FILE* file)
    {
        size_t numPSSKb = 0;
        size_t numDirtyKb = 0;
        if (file)
        {
            rewind(file);
            char line[4096] = { 0 };
            while (fgets(line, sizeof (line), file))
            {
                const char *value;
                // Shared_Dirty is accounted for by forkit's RSS
                if ((value = startsWith(line, "Private_Dirty:")))
                {
                    numDirtyKb += atoi(value);
                }
                else if ((value = startsWith(line, "Pss:")))
                {
                    numPSSKb += atoi(value);
                }
            }
        }

        return std::make_pair(numPSSKb, numDirtyKb);
    }

    std::string getMemoryStats(FILE* file)
    {
        const auto pssAndDirtyKb = getPssAndDirtyFromSMaps(file);
        std::ostringstream oss;
        oss << "procmemstats: pid=" << getpid()
            << " pss=" << pssAndDirtyKb.first
            << " dirty=" << pssAndDirtyKb.second;
        LOG_TRC("Collected " << oss.str());
        return oss.str();
    }

    size_t getMemoryUsagePSS(const Poco::Process::PID pid)
    {
        if (pid > 0)
        {
            const auto cmd = "/proc/" + std::to_string(pid) + "/smaps";
            FILE* fp = fopen(cmd.c_str(), "r");
            if (fp != nullptr)
            {
                const auto pss = getPssAndDirtyFromSMaps(fp).first;
                fclose(fp);
                return pss;
            }
        }

        return 0;
    }

    size_t getMemoryUsageRSS(const Poco::Process::PID pid)
    {
        static const auto pageSizeBytes = getpagesize();
        size_t rss = 0;

        if (pid > 0)
        {
            rss = getStatFromPid(pid, 23);
            rss *= pageSizeBytes;
            rss /= 1024;
            return rss;
        }
        return 0;
    }

    size_t getCpuUsage(const Poco::Process::PID pid)
    {
        if (pid > 0)
        {
            size_t totalJiffies = 0;
            totalJiffies += getStatFromPid(pid, 13);
            totalJiffies += getStatFromPid(pid, 14);
            return totalJiffies;
        }
        return 0;
    }

    size_t getStatFromPid(const Poco::Process::PID pid, int ind)
    {
        if (pid > 0)
        {
            const auto cmd = "/proc/" + std::to_string(pid) + "/stat";
            FILE* fp = fopen(cmd.c_str(), "r");
            if (fp != nullptr)
            {
                char line[4096] = { 0 };
                if (fgets(line, sizeof (line), fp))
                {
                    const std::string s(line);
                    int index = 1;
                    auto pos = s.find(' ');
                    while (pos != std::string::npos)
                    {
                        if (index == ind)
                        {
                            fclose(fp);
                            return strtol(&s[pos], nullptr, 10);
                        }
                        ++index;
                        pos = s.find(' ', pos + 1);
                    }
                }
            }
        }
        return 0;
    }

    std::string replace(std::string result, const std::string& a, const std::string& b)
    {
        const size_t aSize = a.size();
        if (aSize > 0)
        {
            const size_t bSize = b.size();
            std::string::size_type pos = 0;
            while ((pos = result.find(a, pos)) != std::string::npos)
            {
                result = result.replace(pos, aSize, b);
                pos += bSize; // Skip the replacee to avoid endless recursion.
            }
        }

        return result;
    }

    std::string formatLinesForLog(const std::string& s)
    {
        std::string r;
        std::string::size_type n = s.size();
        if (n > 0 && s.back() == '\n')
            r = s.substr(0, n-1);
        else
            r = s;
        return replace(r, "\n", " / ");
    }

    static __thread char ThreadName[32];

    void setThreadName(const std::string& s)
    {
        strncpy(ThreadName, s.c_str(), 31);
        ThreadName[31] = '\0';
        if (prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(s.c_str()), 0, 0, 0) != 0)
            LOG_SYS("Cannot set thread name of " << getThreadId() << " (0x" <<
                    std::hex << std::this_thread::get_id() <<
                    std::dec << ") to [" << s << "].");
        else
            LOG_INF("Thread " << getThreadId() << " (0x" <<
                    std::hex << std::this_thread::get_id() <<
                    std::dec << ") is now called [" << s << "].");
    }

    const char *getThreadName()
    {
        // Main process and/or not set yet.
        if (ThreadName[0] == '\0')
        {
            if (prctl(PR_GET_NAME, reinterpret_cast<unsigned long>(ThreadName), 0, 0, 0) != 0)
                ThreadName[0] = '\0';
        }

        // Avoid so many redundant system calls
        return ThreadName;
    }

    static __thread pid_t ThreadTid;

    pid_t getThreadId()
    {
        // Avoid so many redundant system calls
        if (!ThreadTid)
            ThreadTid = syscall(SYS_gettid);
        return ThreadTid;
    }

    void getVersionInfo(std::string& version, std::string& hash)
    {
        version = std::string(LOOLWSD_VERSION);
        hash = std::string(LOOLWSD_VERSION_HASH);
        hash.resize(std::min(8, (int)hash.length()));
    }

    std::string UniqueId()
    {
        static std::atomic_int counter(0);
        return std::to_string(Poco::Process::id()) + '/' + std::to_string(counter++);
    }

    std::map<std::string, std::string> JsonToMap(const std::string& jsonString)
    {
        Poco::JSON::Parser parser;
        const auto result = parser.parse(jsonString);
        const auto& json = result.extract<Poco::JSON::Object::Ptr>();

        std::vector<std::string> names;
        json->getNames(names);

        std::map<std::string, std::string> map;
        for (const auto& name : names)
        {
            map[name] = json->get(name).toString();
        }

        return map;
    }
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
