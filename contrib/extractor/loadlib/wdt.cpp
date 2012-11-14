#define _CRT_SECURE_NO_DEPRECATE

#include "wdt.h"

bool wdt_MWMO::prepareLoadedData()
{
    if (!compareHeaderName(header, "MWMO"))
        return false;

    return true;
}

bool wdt_MPHD::prepareLoadedData()
{
    if (!compareHeaderName(header, "MPHD"))
        return false;
    return true;
}

bool wdt_MAIN::prepareLoadedData()
{
    if (!compareHeaderName(header, "MAIN"))
        return false;
    return true;
}

bool wdt_MODF::prepareLoadedData()
{
    if (!compareHeaderName(header, "MODF"))
        return false;
    return true;
}

WDT_file::WDT_file()
{
}

WDT_file::~WDT_file()
{
    free();
}

void WDT_file::free()
{
    FileLoader::free();
}

bool WDT_file::prepareLoadedData()
{
    // Check parent
    if (!FileLoader::prepareLoadedData())
        return false;

    if (!getMPHD()->prepareLoadedData())
    {
        printf("ERROR: MPHD-Check failed\n");
        return false;
    }
    if (!getMAIN()->prepareLoadedData())
    {
        printf("ERROR: MAIN-Check failed\n");
        return false;
    }
    if (!getMWMO()->prepareLoadedData())
    {
        printf("ERROR: MWMO-Check failed\n");
        return false;
    }
    if (getMODF() && !getMODF()->prepareLoadedData())
    {
        printf("ERROR: MODF-Check failed\n");
        return false;
    }
    return true;
}

bool WDT_file::hasMODF()
{
    // maybe we have a flag
    uint32 possibleSize = sizeof(file_MVER) + sizeof(ChunkHeader) * 3 +
            getMPHD()->header.size +
            getMAIN()->header.size +
            getMWMO()->header.size;
    return possibleSize < getDataSize();
}
