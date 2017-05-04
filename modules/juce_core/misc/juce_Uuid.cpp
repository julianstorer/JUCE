/*
  ==============================================================================

   This file is part of the JUCE library.
   Copyright (c) 2017 - ROLI Ltd.

   JUCE is an open source library subject to commercial or open-source
   licensing.

   The code included in this file is provided under the terms of the ISC license
   http://www.isc.org/downloads/software-support-policy/isc-license. Permission
   To use, copy, modify, and/or distribute this software for any purpose with or
   without fee is hereby granted provided that the above copyright notice and
   this permission notice appear in all copies.

   JUCE IS PROVIDED "AS IS" WITHOUT ANY WARRANTY, AND ALL WARRANTIES, WHETHER
   EXPRESSED OR IMPLIED, INCLUDING MERCHANTABILITY AND FITNESS FOR PURPOSE, ARE
   DISCLAIMED.

  ==============================================================================
*/

Uuid::Uuid()
{
    Random r;

    for (size_t i = 0; i < sizeof (uuid); ++i)
        uuid[i] = (uint8) (r.nextInt (256));

    // To make it RFC 4122 compliant, need to force a few bits...
    uuid[6] = (uuid[6] & 0x0f) | 0x40;
    uuid[8] = (uuid[8] & 0x3f) | 0x80;
}

Uuid::~Uuid() noexcept {}

Uuid::Uuid (const Uuid& other) noexcept
{
    memcpy (uuid, other.uuid, sizeof (uuid));
}

Uuid& Uuid::operator= (const Uuid& other) noexcept
{
    memcpy (uuid, other.uuid, sizeof (uuid));
    return *this;
}

int Uuid::getTimeLow (const Uuid& uuid) noexcept                    { return ((int) uuid.uuid[15] << 24) + ((int) uuid.uuid[14] << 16) + ((int) uuid.uuid[13] << 8) + (int) uuid.uuid[12]; }
int16 Uuid::getTimeMid (const Uuid& uuid) noexcept                  { return ((int16) uuid.uuid[11] << 8) + (int16) uuid.uuid[10]; }
int16 Uuid::getTimeHiAndVersion (const Uuid& uuid) noexcept         { return ((int16) uuid.uuid[9] << 8) + (int16) uuid.uuid[8]; }
uint8 Uuid::getClockSeqHiAndReserved (const Uuid& uuid) noexcept    { return uuid.uuid[9]; }
uint8 Uuid::getClockLow (const Uuid& uuid) noexcept                 { return uuid.uuid[8]; }

int Uuid::compare (const Uuid& a, const Uuid& b) noexcept
{
    if (int r = check<int, getTimeLow> (a, b))                      return r;
    if (int16 r = check<int16, getTimeMid> (a, b))                  return r;
    if (int16 r = check<int16, getTimeHiAndVersion> (a, b))         return r;
    if (uint8 r = check<uint8, getClockSeqHiAndReserved> (a, b))    return r;
    if (uint8 r = check<uint8, getClockLow> (a, b))                 return r;

    for (size_t i = 0; i <= 6; ++i)
    {
        if (a.uuid[i] < b.uuid[i])
            return -1;

        if (a.uuid[i] > b.uuid[i])
            return 1;
    }

    return 0;
}

bool Uuid::operator== (const Uuid& other) const noexcept    { return memcmp (uuid, other.uuid, sizeof (uuid)) == 0; }
bool Uuid::operator!= (const Uuid& other) const noexcept    { return ! operator== (other); }
bool Uuid::operator< (const Uuid& other) const noexcept     { return compare (*this, other) < 0; }
bool Uuid::operator<= (const Uuid& other) const noexcept    { return compare (*this, other) <= 0; }
bool Uuid::operator> (const Uuid& other) const noexcept     { return compare (*this, other) > 0; }
bool Uuid::operator>= (const Uuid& other) const noexcept    { return compare (*this, other) >= 0; }

Uuid Uuid::null() noexcept
{
    return Uuid ((const uint8*) nullptr);
}

bool Uuid::isNull() const noexcept
{
    for (size_t i = 0; i < sizeof (uuid); ++i)
        if (uuid[i] != 0)
            return false;

    return true;
}

String Uuid::getHexRegion (int start, int length) const
{
    return String::toHexString (uuid + start, length, 0);
}

String Uuid::toString() const
{
    return getHexRegion (0, 16);
}

String Uuid::toDashedString() const
{
    return getHexRegion (0, 4)
            + "-" + getHexRegion (4, 2)
            + "-" + getHexRegion (6, 2)
            + "-" + getHexRegion (8, 2)
            + "-" + getHexRegion (10, 6);
}

Uuid::Uuid (const String& uuidString)
{
    operator= (uuidString);
}

Uuid& Uuid::operator= (const String& uuidString)
{
    MemoryBlock mb;
    mb.loadFromHexString (uuidString);
    mb.ensureSize (sizeof (uuid), true);
    mb.copyTo (uuid, 0, sizeof (uuid));
    return *this;
}

Uuid::Uuid (const uint8* const rawData) noexcept
{
    operator= (rawData);
}

Uuid& Uuid::operator= (const uint8* const rawData) noexcept
{
    if (rawData != nullptr)
        memcpy (uuid, rawData, sizeof (uuid));
    else
        zeromem (uuid, sizeof (uuid));

    return *this;
}
