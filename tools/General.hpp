/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <string>
#include <map>
#include <iostream>

// This namespace is header only.

namespace GeneralTools{
    /// Splits string into vector<string>. Does not accept referenced variables for easy usage like (splitString("test", ..)) or (splitString(getStringOnTheFly(), ..))
    inline std::vector<std::string> splitStringToVector(std::string const str, const char delim)
    {
        size_t start;
        size_t end = 0;

        std::vector<std::string> result;

        while ((start = str.find_first_not_of(delim, end)) != std::string::npos)
        {
            end = str.find(delim, start);
            result.push_back(str.substr(start, end - start));
        }
        return result;
    }
    
    /// Converts vector of strings to map. Strings should have formed like this: key + delimiter + value.
    /// In case of a misformed string or zero length vector, returns an empty map.
    inline std::map<std::string, std::string> stringVectorToMap(std::vector<std::string> sVector, const char delimiter)
    {
        std::map<std::string, std::string> result;

        for(std::vector<std::string>::iterator it = sVector.begin(); it != sVector.end(); it++){
            size_t delimiterPosition = 0;
            delimiterPosition = (*it).find(delimiter, 0);
            if(delimiterPosition != std::string::npos){
                std::string key = (*it).substr(0, delimiterPosition);
                delimiterPosition++;
                std::string value = (*it).substr(delimiterPosition);
                result[key] = value;
            }
            else{
                return std::map<std::string, std::string>();
            }
        }
        
        return result;
    }
}