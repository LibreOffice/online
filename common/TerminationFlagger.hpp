/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef INCLUDED_TERMINATIONFLAGGER_HPP
#define INCLUDED_TERMINATIONFLAGGER_HPP

#include "SigUtil.hpp"

class TerminationFlaggerInterface
{
public:
    virtual bool getTerminationFlag() const = 0;

    virtual void setTerminationFlag() = 0;
};

class TrivialTerminationFlagger : public virtual TerminationFlaggerInterface
{
public:
    TrivialTerminationFlagger()
        : _flag(false)
    {
    }

    bool getTerminationFlag() const override
    {
        return _flag || SigUtil::getTerminationSignalled();
    }

    void setTerminationFlag() override
    {
        _flag = true;
    }

private:
    bool _flag;
};

#endif

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
