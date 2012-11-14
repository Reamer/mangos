#ifndef ADT_H
#define ADT_H

#include <vector>
#include <string>
#include "loadlib.h"
#include "../util.h"

#define TILESIZE (533.33333f)
#define CHUNKSIZE ((TILESIZE) / 16.0f)
#define UNITSIZE (CHUNKSIZE / 8.0f)

enum LiquidType
{
    LIQUID_TYPE_WATER = 0,
    LIQUID_TYPE_OCEAN = 1,
    LIQUID_TYPE_MAGMA = 2,
    LIQUID_TYPE_SLIME = 3
};

//**************************************************************************************
// ADT file class
//**************************************************************************************
#define ADT_CELLS_PER_GRID    16
#define ADT_CELL_SIZE         8
#define ADT_GRID_SIZE         (ADT_CELLS_PER_GRID*ADT_CELL_SIZE)


bool isHole(int holes, int i, int j);

struct MODF_Entry
{
    uint32 mwidEntry;                  // (index in the MWID list)
    uint32 uniqueId;            // unique identifier for this instance
    float position[3];
    float rotation[3];
    float upperExtents[3];
    float lowerExtents[3];
    uint16 flags;
    uint16 doodadSet;
    uint16 nameSet;
    uint16 padding;
    MODF_Entry()
    {
        mwidEntry = 0;
        uniqueId = 0;
        position[0] = 0.0f;
        position[1] = 0.0f;
        position[2] = 0.0f;
        rotation[0] = 0.0f;
        rotation[1] = 0.0f;
        rotation[2] = 0.0f;
        upperExtents[0] = 0.0f;
        upperExtents[1] = 0.0f;
        upperExtents[2] = 0.0f;
        lowerExtents[0] = 0.0f;
        lowerExtents[1] = 0.0f;
        lowerExtents[2] = 0.0f;
        flags = 0;
        doodadSet = 0;
        nameSet = 0;
        padding = 0;
    }
    static void printMODF_Entry(MODF_Entry* entry);
};

struct MDDF_Entry
{
    uint32 mmidEntry;
    uint32 uniqueId;
    float position[3];
    float rotation[3];
    uint16 scale;
    uint16 flags;
    MDDF_Entry(){
        mmidEntry = 0;
        uniqueId = 0;
        position[0] = 0.0f;
        position[1] = 0.0f;
        position[2] = 0.0f;
        rotation[0] = 0.0f;
        rotation[1] = 0.0f;
        rotation[2] = 0.0f;
        scale = 0;
        flags = 0;
    }
    static void printMDDF_Entry(MDDF_Entry* entry);
};


//
// Adt file height map chunk
//
class adt_MCVT
{
    private:
        ChunkHeader header;
    public:
        float height_map[(ADT_CELL_SIZE+1)*(ADT_CELL_SIZE+1)+ADT_CELL_SIZE*ADT_CELL_SIZE];

        bool  prepareLoadedData();
};

//
// Adt file liquid map chunk (old)
//
class adt_MCLQ
{
    private:
        ChunkHeader header;
    public:
        float height1;
        float height2;
        struct liquid_data{
            uint32 light;
            float  height;
        } liquid[ADT_CELL_SIZE+1][ADT_CELL_SIZE+1];

        // 1<<0 - ochen
        // 1<<1 - lava/slime
        // 1<<2 - water
        // 1<<6 - all water
        // 1<<7 - dark water
        // == 0x0F - not show liquid
        uint8 flags[ADT_CELL_SIZE][ADT_CELL_SIZE];
        uint8 data[84];
        bool  prepareLoadedData();
};

//
// Adt file cell chunk
//
class adt_MCNK
{
    private:
        ChunkHeader header;
    public:
        uint32 flags;
        uint32 ix;
        uint32 iy;
        uint32 nLayers;
        uint32 nDoodadRefs;
        uint32 offsMCVT;        // height map
        uint32 offsMCNR;        // Normal vectors for each vertex
        uint32 offsMCLY;        // Texture layer definitions
        uint32 offsMCRF;        // A list of indices into the parent file's MDDF chunk
        uint32 offsMCAL;        // Alpha maps for additional texture layers
        uint32 sizeMCAL;
        uint32 offsMCSH;        // Shadow map for static shadows on the terrain
        uint32 sizeMCSH;
        uint32 areaid;
        uint32 nMapObjRefs;
        uint16 holes;           // locations where models pierce the heightmap
        uint16 pad;
        uint16 s[2];
        uint32 data1;
        uint32 data2;
        uint32 data3;
        uint32 predTex;
        uint32 nEffectDoodad;
        uint32 offsMCSE;
        uint32 nSndEmitters;
        uint32 offsMCLQ;         // Liqid level (old)
        uint32 sizeMCLQ;         //
        float  zpos;
        float  xpos;
        float  ypos;
        uint32 offsMCCV;         // offsColorValues in WotLK
        uint32 props;
        uint32 effectId;

        bool   prepareLoadedData();
        adt_MCVT* getMCVT()
        {
            if (offsMCVT)
                return (adt_MCVT *)((uint8 *)this + offsMCVT);
            return 0;
        }
        adt_MCLQ* getMCLQ()
        {
            if (offsMCLQ)
                return (adt_MCLQ *)((uint8 *)this + offsMCLQ);
            return 0;
        }
};

//
// Adt file grid chunk
//
class adt_MCIN
{
    private:
        ChunkHeader header;
    public:
        struct adt_CELLS{
            uint32 offsMCNK;
            uint32 size;
            uint32 flags;
            uint32 asyncId;
        } cells[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];

        bool   prepareLoadedData();
        // offset from begin file (used this-84)
        // 84 = sizeof(adt_MHDR)+sizeof(file_MVER)
        adt_MCNK* getMCNK(int x, int y)
        {
            if (cells[x][y].offsMCNK)
                return (adt_MCNK *)((uint8 *)this - 84 + cells[x][y].offsMCNK);
            return 0;
        }
};

struct MH2O_HeightmapData
{
    // if type & 1 != 1, this chunk is "ocean".  in this case, do not use this structure.

    float heightMap[];  // w*h
    char transparency[];    // w*h
};

#define ADT_LIQUID_HEADER_FULL_LIGHT   0x01
#define ADT_LIQUID_HEADER_NO_HIGHT     0x02

struct adt_liquid_header{
    uint16 liquidType;             // Index from LiquidType.dbc
    uint16 formatFlags;
    float  heightLevel1;
    float  heightLevel2;
    uint8  xOffset;
    uint8  yOffset;
    uint8  width;
    uint8  height;
    uint32 offsLiquidShow;
    uint32 offsHeightmap;
};

//
// Adt file liquid data chunk (new)
//
class adt_MH2O
{
    public:
        ChunkHeader header;

        struct adt_LIQUID{
            uint32 offsInformation;
            uint32 used;
            uint32 offsRender;
        } liquid[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];

        bool   prepareLoadedData();

        adt_liquid_header* getLiquidData(int x, int y)
        {
            if (liquid[x][y].used && liquid[x][y].offsInformation)
                return (adt_liquid_header*)((uint8*)this + 8 + liquid[x][y].offsInformation);
            return 0;
        }

        float* getLiquidHeightMap(adt_liquid_header* h)
        {
            if (h->formatFlags & ADT_LIQUID_HEADER_NO_HIGHT)
                return 0;
            if (h->offsHeightmap)
                return (float*)((uint8*)this + 8 + h->offsHeightmap);
            return 0;
        }

        uint8* getLiquidLightMap(adt_liquid_header* h)
        {
            if (h->formatFlags & ADT_LIQUID_HEADER_FULL_LIGHT)
                return 0;
            if (h->offsHeightmap)
            {
                if (h->formatFlags & ADT_LIQUID_HEADER_NO_HIGHT)
                    return (uint8 *)((uint8*)this + 8 + h->offsHeightmap);
                return (uint8 *)((uint8*)this + 8 + h->offsHeightmap + (h->width+1)*(h->height+1)*4);
            }
            return 0;
        }

        uint32* getLiquidFullLightMap(adt_liquid_header* h)
        {
            if (!(h->formatFlags & ADT_LIQUID_HEADER_FULL_LIGHT))
                return 0;
            if (h->offsHeightmap)
            {
                if (h->formatFlags & ADT_LIQUID_HEADER_NO_HIGHT)
                    return (uint32 *)((uint8*)this + 8 + h->offsHeightmap);
                return (uint32 *)((uint8*)this + 8 + h->offsHeightmap + (h->width+1)*(h->height+1)*4);
            }
            return 0;
        }

        uint64 getLiquidShowMap(adt_liquid_header* h)
        {
            if (h->offsLiquidShow)
                return *((uint64 *)((uint8*)this + 8 + h->offsLiquidShow));
            else
                return 0xFFFFFFFFFFFFFFFFLL;
        }

};

struct FilenameInfo
{
    uint32 offset;
    uint32 length;
    std::string filename;
    FilenameInfo()
    {
        offset = 0;
        length = 0;
        filename = "";
    }
};

class adt_MMDX
{
    private:
        ChunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<char*> getFileNames();
        void getM2Model(FilenameInfo* info);
};

class adt_MMID
{
    private:
        ChunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<uint32> getOffsetList();
        FilenameInfo getMMDXInfo(MDDF_Entry* entry) { return getMMDXInfo(entry->mmidEntry); }
        FilenameInfo getMMDXInfo(uint32 value);
        inline uint32 getMaxM2Models() { return header.size/sizeof(uint32);}
};

class adt_MWMO
{
    private:
        ChunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<char*> getFileNames();
        void getWMOFilename(FilenameInfo* info);
};

class adt_MWID
{
    private:
        ChunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<uint32> getOffsetList();
        FilenameInfo getMWMOInfo(MODF_Entry* entry) { return getMWMOInfo(entry->mwidEntry);};
        FilenameInfo getMWMOInfo(uint32 value);
        inline uint32 getMaxWMO() { return header.size/sizeof(uint32); }
};

class adt_MDDF
{
        ChunkHeader header;
    public:
        bool prepareLoadedData();
        inline uint32 getMaxEntries() { return header.size/sizeof(MDDF_Entry);}
        MDDF_Entry getMDDF_Entry(uint32 value);
};

class adt_MODF
{
    private:
        ChunkHeader header;
    public:
        bool prepareLoadedData();
        inline uint32 getMaxEntries() { return header.size/sizeof(MODF_Entry);}
        MODF_Entry getMODF_Entry(uint32 value);
};

//
// Adt file header chunk
//
class adt_MHDR
{
    ChunkHeader header;

    uint32 pad;
    uint32 offsMCIN;           // MCIN
    uint32 offsTex;	           // MTEX
    uint32 offsModels;	       // MMDX
    uint32 offsModelsIds;	   // MMID
    uint32 offsMapObejcts;	   // MWMO
    uint32 offsMapObejctsIds;  // MWID
    uint32 offsDoodsDef;       // MDDF
    uint32 offsObjectsDef;     // MODF
    uint32 offsMFBO;           // MFBO
    uint32 offsMH2O;           // MH2O
    uint32 data1;              // MTXF
    uint32 data2;
    uint32 data3;
    uint32 data4;
    uint32 data5;
public:
    bool prepareLoadedData();
    adt_MCIN* getMCIN(){ return offsMCIN ? (adt_MCIN *)((uint8 *)&pad+offsMCIN) : 0;}
    adt_MH2O* getMH2O(){ return offsMH2O ? (adt_MH2O *)((uint8 *)&pad+offsMH2O) : 0;}

    // m2 or mdx models (doodads)
    adt_MMDX* getMMDX(){ return offsModels ? (adt_MMDX *)((uint8 *)&pad+offsModels) : 0;}
    adt_MMID* getMMID(){ return offsModelsIds ? (adt_MMID *)((uint8 *)&pad+offsModelsIds) : 0;}
    adt_MDDF* getMDDF(){ return offsDoodsDef ? (adt_MDDF *)((uint8 *)&pad+offsDoodsDef) : 0;}

    // wmo objects
    adt_MWMO* getMWMO(){ return offsMapObejcts ? (adt_MWMO *)((uint8 *)&pad+offsMapObejcts) : 0;}
    adt_MWID* getMWID(){ return offsMapObejctsIds ? (adt_MWID *)((uint8 *)&pad+offsMapObejctsIds) : 0;}
    adt_MODF* getMODF(){ return offsObjectsDef ? (adt_MODF *)((uint8 *)&pad+offsObjectsDef) : 0;}


};

class ADT_file : public FileLoader{
    public:
        bool prepareLoadedData();
        ADT_file();
        ~ADT_file();
        void free();

        adt_MHDR* getMHDR() {return (adt_MHDR *)(getData()+sizeof(file_MVER));}
};

#endif
