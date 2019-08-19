/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4; fill-column: 100 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef INCLUDED_LOKITHELPER_HPP
#define INCLUDED_LOKITHELPER_HPP

#include <sstream>
#include <string>

#include <Util.hpp>

#define LOK_USE_UNSTABLE_API
#include <LibreOfficeKit/LibreOfficeKitEnums.h>

namespace LOKitHelper
{
    inline std::string documentTypeToString(LibreOfficeKitDocumentType type)
    {
        switch (type)
        {
        case LOK_DOCTYPE_TEXT:
            return "text";
        case LOK_DOCTYPE_SPREADSHEET:
            return "spreadsheet";
        case LOK_DOCTYPE_PRESENTATION:
            return "presentation";
        case LOK_DOCTYPE_DRAWING:
            return "drawing";
        default:
            return "other-" + std::to_string(type);
        }
    }

    inline std::string getDocumentTypeAsString(LibreOfficeKitDocument *loKitDocument)
    {
        assert(loKitDocument && "null loKitDocument");
        const auto type = static_cast<LibreOfficeKitDocumentType>(loKitDocument->pClass->getDocumentType(loKitDocument));
        return documentTypeToString(type);
    }

    inline std::string documentStatus(LibreOfficeKitDocument *loKitDocument)
    {
        char *ptrValue;
        assert(loKitDocument && "null loKitDocument");
        const auto type = static_cast<LibreOfficeKitDocumentType>(loKitDocument->pClass->getDocumentType(loKitDocument));

        const int parts = loKitDocument->pClass->getParts(loKitDocument);
        std::ostringstream oss;
        oss << "type=" << documentTypeToString(type)
            << " parts=" << parts
            << " current=" << loKitDocument->pClass->getPart(loKitDocument);

        long width, height;
        loKitDocument->pClass->getDocumentSize(loKitDocument, &width, &height);
        oss << " width=" << width
            << " height=" << height
            << " viewid=" << loKitDocument->pClass->getView(loKitDocument);

        if (type == LOK_DOCTYPE_SPREADSHEET || type == LOK_DOCTYPE_PRESENTATION)
        {
            if (type == LOK_DOCTYPE_SPREADSHEET)
            {
                std::ostringstream hposs;
                for (int i = 0; i < parts; ++i)
                {

                    ptrValue = loKitDocument->pClass->getPartInfo(loKitDocument, i);
                    std::string partinfo(ptrValue);
                    std::free(ptrValue);
                    const auto aPartInfo = Util::JsonToMap(partinfo);
                    for (const auto& prop: aPartInfo)
                    {
                        const std::string& name = prop.first;
                        if (name == "visible")
                        {
                            if (prop.second == "0")
                                hposs << i << ",";
                        }
                    }
                }
                std::string hiddenparts = hposs.str();
                if (!hiddenparts.empty())
                {
                    hiddenparts.pop_back();
                    oss << " hiddenparts=" << hiddenparts;
                }
            }

            for (int i = 0; i < parts; ++i)
            {
                oss << "\n";
                ptrValue = loKitDocument->pClass->getPartName(loKitDocument, i);
                oss << ptrValue;
                std::free(ptrValue);
            }

            if (type == LOK_DOCTYPE_PRESENTATION)
            {
                for (int i = 0; i < parts; ++i)
                {
                    oss << "\n";
                    ptrValue = loKitDocument->pClass->getPartHash(loKitDocument, i);
                    oss << ptrValue;
                    std::free(ptrValue);
                }
            }
        }

        return oss.str();
    }
}

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
