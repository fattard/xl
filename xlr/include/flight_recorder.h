#ifndef FLIGHT_RECORDER_H
#define FLIGHT_RECORDER_H
// *****************************************************************************
// flight_recorder.h                                                  XL project
// *****************************************************************************
//
// File description:
//
//     Record information about what's going on in the application
//
//
//
//
//
//
//
//
// *****************************************************************************
// This software is licensed under the GNU General Public License v3
// (C) 2011,2019, Christophe de Dinechin <christophe@dinechin.org>
// (C) 2012, Jérôme Forissier <jerome@taodyne.com>
// *****************************************************************************
// This file is part of XL
//
// XL is free software: you can r redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// XL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with XL, in a file named COPYING.
// If not, see <https://www.gnu.org/licenses/>.
// *****************************************************************************

#include "base.h"
#include <vector>

XL_BEGIN

// ============================================================================
//
//    Higher-evel interface
//
// ============================================================================

enum FlightRecorderChannels
// ----------------------------------------------------------------------------
//   Different channels that can be recorded
// ----------------------------------------------------------------------------
{
    // General enablers
    REC_ALWAYS             = 1<<0,
    REC_CRITICAL           = 1<<1,
    REC_DEBUG              = 1<<2,
    REC_INFO               = 1<<3,

    // Domain-specific enablers
    REC_MEMORY_DETAILS     = 1<<8,
    REC_COMPILER_DETAILS   = 1<<9,
    REC_EVAL_DETAILS       = 1<<10,
    REC_PRIMITIVES_DETAILS = 1<<11,

    // High-level enablers
    REC_MEMORY             = REC_DEBUG | REC_MEMORY_DETAILS,
    REC_COMPILER           = REC_DEBUG | REC_COMPILER_DETAILS,
    REC_EVAL               = REC_DEBUG | REC_EVAL_DETAILS,
    REC_PRIMITIVES         = REC_DEBUG | REC_PRIMITIVES_DETAILS,
};


struct FlightRecorder
// ----------------------------------------------------------------------------
//    Record events
// ----------------------------------------------------------------------------
{
    struct Entry
    {
        Entry(kstring what = "", void *caller = NULL,
              kstring l1="", intptr_t a1=0,
              kstring l2="", intptr_t a2=0,
              kstring l3="", intptr_t a3=0):
            what(what), caller(caller),
            label1(l1), label2(l2), label3(l3),
            arg1(a1), arg2(a2), arg3(a3) {}
        kstring  what;
        void *   caller;
        kstring  label1, label2, label3;
        intptr_t arg1, arg2, arg3;
    };

    FlightRecorder(uint size=4096) : windex(0), rindex(0),
                                     records(size) {}

public:
    // Interface for a given recorder
    ulong Record(kstring what, void *caller = NULL,
                 kstring l1="", intptr_t a1=0,
                 kstring l2="", intptr_t a2=0,
                 kstring l3="", intptr_t a3=0)
    {
        Entry &e = records[windex++ % records.size()];
        e.what = what;
        e.caller = caller;
        e.label1 = l1;
        e.arg1 = a1;
        e.label2 = l2;
        e.arg2 = a2;
        e.label3 = l3;
        e.arg3 = a3;
        return enabled;
    }

    void Dump(int fd, bool consume = false);
    void Resize(uint size) { records.resize(size); }

public:
    // Static interface
    static void Initialize()   { if (!recorder) recorder = new FlightRecorder; }
    static ulong SRecord(kstring what, void *caller = NULL,
                         kstring l1="", intptr_t a1=0,
                         kstring l2="", intptr_t a2=0,
                         kstring l3="", intptr_t a3=0)
    {
        return recorder->Record(what, caller, l1, a1, l2, a2, l3, a3);
    }
    static void SDump(int fd, bool kill=false) { recorder->Dump(fd,kill); }
    static void SResize(uint size) { recorder->Resize(size); }
    static void SFlags(ulong en) { enabled = en | REC_ALWAYS; }

public:
    uint               windex, rindex;
    std::vector<Entry> records;

    static ulong            enabled;

private:
    static FlightRecorder * recorder;
};


#define RECORD(cond, what, args...)                                     \
    ((XL::REC_##cond) & (XL::FlightRecorder::enabled | XL::REC_ALWAYS)  \
     && XL::FlightRecorder::SRecord(what,                               \
                                    __builtin_return_address(0),        \
                                    ##args))

XL_END

// For use within a debugger session
extern void recorder_dump();

#endif // FLIGHT_RECORDER_H
