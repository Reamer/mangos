#ifndef WDT_H
#define WDT_H
#include "loadlib.h"
#include <vector>
#include <string>
#include "adt.h"

//**************************************************************************************
// WDT file class and structures
//**************************************************************************************
#define WDT_MAP_SIZE 64

class wdt_MODF
{
    public:
        chunkHeader wdtMODFHeader;
        MODF_Entry modf;

        bool prepareLoadedData();
};

class wdt_MWMO{

    public:
        chunkHeader wdtMWMOHeader;
        bool prepareLoadedData();
        std::vector<char*> getFileNames()
        {
            std::vector<char*> result;
            result.clear();
            if (wdtMWMOHeader.size)
            {
                char allFilenames[wdtMWMOHeader.size];
                memcpy(allFilenames, ((uint8*)this + sizeof(chunkHeader)), wdtMWMOHeader.size);
                result = splitFileNamesAtDelim(allFilenames, wdtMWMOHeader.size, '\0');
            }
            return result;
        }
};

class wdt_MPHD{
    public:
        chunkHeader wdtMPHDHeader;

        uint32 data1;
        uint32 data2;
        uint32 data3;
        uint32 data4;
        uint32 data5;
        uint32 data6;
        uint32 data7;
        uint32 data8;
        bool   prepareLoadedData();
};

class wdt_MAIN{
    public:
        chunkHeader wdtMainHeader;

        struct adtData{
            uint32 exist;
            uint32 data1;
        } adt_list[64][64];

        bool   prepareLoadedData();
};

class WDT_file : public FileLoader{
    public:
        bool   prepareLoadedData();

        WDT_file();
        virtual ~WDT_file();
        void free();

        wdt_MPHD* getMPHD() {return (wdt_MPHD*)(GetData() + sizeof(file_MVER)); };
        wdt_MAIN* getMAIN() {return (wdt_MAIN*)((uint8*)getMPHD() + sizeof(chunkHeader) + getMPHD()->wdtMPHDHeader.size); };
        wdt_MWMO* getMWMO() {return (wdt_MWMO*)((uint8*)getMAIN() + sizeof(chunkHeader) + getMAIN()->wdtMainHeader.size); };
        wdt_MODF* getMODF() {return (wdt_MODF*)(hasMODF() ? (uint8*)getMWMO() + sizeof(chunkHeader) + getMWMO()->wdtMWMOHeader.size : NULL); };
        bool hasMODF();
};

#endif
