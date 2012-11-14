#ifndef WDT_H
#define WDT_H
#include "loadlib.h"
#include <vector>
#include <string>
#include "adt.h"
#include "../util.h"

//**************************************************************************************
// WDT file class and structures
//**************************************************************************************
#define WDT_MAP_SIZE 64

class wdt_MODF
{
    public:
        ChunkHeader header;
        MODF_Entry modf;

        bool prepareLoadedData();
};

class wdt_MWMO{

    public:
        ChunkHeader header;
        bool prepareLoadedData();
        void fillFileName(char* &filename)
        {
            if (header.size)
            {
                filename = new char[header.size];
                memcpy(filename, ((uint8*)this + sizeof(ChunkHeader)), header.size);
            }
        }

};

class wdt_MPHD{
    public:
        ChunkHeader header;

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
        ChunkHeader header;

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

        wdt_MPHD* getMPHD() {return (wdt_MPHD*)(getData() + sizeof(file_MVER)); };
        wdt_MAIN* getMAIN() {return (wdt_MAIN*)((uint8*)getMPHD() + sizeof(ChunkHeader) + getMPHD()->header.size); };
        wdt_MWMO* getMWMO() {return (wdt_MWMO*)((uint8*)getMAIN() + sizeof(ChunkHeader) + getMAIN()->header.size); };
        wdt_MODF* getMODF() {return (wdt_MODF*)(hasMODF() ? (uint8*)getMWMO() + sizeof(ChunkHeader) + getMWMO()->header.size : NULL); };
        bool hasMODF();
};

#endif
