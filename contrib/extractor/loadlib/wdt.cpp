#define _CRT_SECURE_NO_DEPRECATE

#include "wdt.h"

bool wdt_MWMO::prepareLoadedData()
{
    if (wdtMWMOHeader.fcc != 'MWMO')
        return false;

    if (getFileNames().size() > 1)
    {
        printf("ERROR: MWMO-Check failed. More then one file in WDT");
        return false;
    }
    return true;
}

bool wdt_MPHD::prepareLoadedData()
{
    if (wdtMPHDHeader.fcc != 'MPHD')
        return false;
    return true;
}

bool wdt_MAIN::prepareLoadedData()
{
    if (wdtMainHeader.fcc != 'MAIN')
        return false;
    return true;
}

bool wdt_MODF::prepareLoadedData()
{
    if (wdtMODFHeader.fcc != 'MODF')
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
    uint32 possibleSize = sizeof(file_MVER) + sizeof(chunkHeader) * 3 +
            getMPHD()->wdtMPHDHeader.size +
            getMAIN()->wdtMainHeader.size +
            getMWMO()->wdtMWMOHeader.size;
    return possibleSize < GetDataSize();
}
