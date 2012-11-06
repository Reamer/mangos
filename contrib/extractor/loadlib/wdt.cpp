#define _CRT_SECURE_NO_DEPRECATE

#include "wdt.h"

bool wdt_MWMO::prepareLoadedData()
{
    if (fcc != 'MWMO')
        return false;
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
        return false;
    if (!getMAIN()->prepareLoadedData())
        return false;
    if (!getMWMO()->prepareLoadedData())
        return false;
    return true;
}
