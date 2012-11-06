#ifndef WDT_H
#define WDT_H
#include "loadlib.h"

//**************************************************************************************
// WDT file class and structures
//**************************************************************************************
#define WDT_MAP_SIZE 64

class wdt_MWMO{
    union{
        uint32 fcc;
        char   fcc_txt[4];
    };
public:
    uint32 size;
    bool prepareLoadedData();
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

        wdt_MPHD* getMPHD() {return (wdt_MPHD*)(GetData() + sizeof(chunkHeader) + version->header.size); };
        wdt_MAIN* getMAIN() {return (wdt_MAIN*)(getMPHD() + sizeof(chunkHeader) + getMPHD()->wdtMPHDHeader.size); };
        wdt_MWMO* getMWMO() {return (wdt_MWMO*)(getMAIN() + sizeof(chunkHeader) + getMAIN()->wdtMainHeader.size); };
};

#endif
