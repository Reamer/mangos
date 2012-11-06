#ifndef ADT_H
#define ADT_H

#include <vector>
#include <string>
#include "loadlib.h"

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
// ignore '\0', hard splitting (in use for split after each '\0'
std::vector<char*> splitFileNamesAtDelim(char* filenames, uint32 size, char delim);
void changeWhitespaceToUnderscore(char* name, size_t len);
void changeWhitespaceToUnderscore(std::string name);
void fixnamen(char* name, size_t len);


//
// Adt file height map chunk
//
class adt_MCVT
{
    private:
        chunkHeader header;
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
        chunkHeader header;
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
        chunkHeader header;
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
        chunkHeader header;
    public:
        struct adt_CELLS{
            uint32 offsMCNK;
            uint32 size;
            uint32 flags;
            uint32 asyncId;
        } cells[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];

        bool   prepareLoadedData();
        // offset from begin file (used this-84)
        adt_MCNK *getMCNK(int x, int y)
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
        chunkHeader header;

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
};
class adt_MMDX
{
    private:
        chunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<char*> getFileNames()
        {
            std::vector<char*> result;
            result.clear();
            if (header.size)
            {
                char allFilenames[header.size];
                memcpy(allFilenames, ((uint8*)this + sizeof(chunkHeader)), header.size);
                result = splitFileNamesAtDelim(allFilenames, header.size, '\0');
            }
            return result;
        }
        void getM2Model(FilenameInfo* info)
        {
            if (!header.size)
                return;

            if (info->offset >= header.size)
            {
                printf("WARNING: offset %u >= size %u\n", info->offset, header.size);
                return;
            }

            // calculate length
            uint32 length;
            if (info->length == 0)
                length = header.size-info->offset;
            else
                length = info->length;


            /*TODO: verify why the length makes problems
            char modelname[length];
            memcpy(modelname, ((uint8*)this + 8 + info->offset), length);
            changeWhitespaceToUnderscore(modelname, length);
            fixnamen(modelname, length);
            info->filename.append(modelname, length);
            */

            char modelname[512];
            memcpy(modelname, ((uint8*)this + sizeof(chunkHeader) + info->offset), 512);
            info->filename.append(modelname);
            return;
        }

};

class adt_MMID
{
    private:
        chunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<uint32> getOffsetList()
        {
            std::vector<uint32> result;
            uint32 values = getMaxM2Models();
            for (uint32 i = 0; i < values; ++i)
            {
                uint32 j;
                memcpy(&j, ((uint8*)this + sizeof(chunkHeader) + sizeof(uint32)*i), sizeof(uint32));
                result.push_back(j);
            }
            return result;
        }
        FilenameInfo getMMDXInfo(uint32 value)
        {
            // create Info
            FilenameInfo info;
            if (header.size)
            {
                memcpy(&info.offset, ((uint8*)this + sizeof(chunkHeader) + sizeof(uint32)*value), sizeof(info.offset));
                value++;
                if (value < getMaxM2Models())
                {
                    uint32 nextoffset;
                    memcpy(&nextoffset, ((uint8*)this + sizeof(chunkHeader) + sizeof(uint32)*value), sizeof(uint32));
                    info.length = nextoffset - info.offset;
                }
            }
            return info;
        }
        uint32 getMaxM2Models()
        {
            return header.size/sizeof(uint32);
        }
};

class adt_MWMO
{
    private:
        chunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<char*> getFileNames()
        {
            std::vector<char*> result;
            result.clear();
            if (header.size)
            {
                char allFilenames[header.size];
                memcpy(allFilenames, ((uint8*)this + 8), header.size);
                result = splitFileNamesAtDelim(allFilenames, header.size, '\0');
            }
            return result;
        }
        void getWMO(FilenameInfo* info)
        {
            if (!header.size)
                return;

            if (info->offset >= header.size)
            {
                printf("WARNING: offset %u >= size %u\n", info->offset, header.size);
                return;
            }

            // calculate length
            uint32 length;
            if (info->length == 0)
                length = header.size-info->offset;
            else
                length = info->length;

            /*TODO: verify why the length makes problems
            char name[length];
            memcpy(name, ((uint8*)this + 8 + info->offset), length);
            changeWhitespaceToUnderscore(name, length);
            fixnamen(name, length);
            info->filename.append(name, length);
            */

            char name[512];
            memcpy(name, ((uint8*)this + 8 + info->offset), 512);
            info->filename.append(name);
            return;
        }

};

class adt_MWID
{
    private:
        chunkHeader header;
    public:
        bool prepareLoadedData();
        std::vector<uint32> getOffsetList()
        {
            std::vector<uint32> result;
            uint32 values = getMaxWMO();
            for (uint32 i = 0; i < values; ++i)
            {
                uint32 j;
                memcpy(&j, ((uint8*)this + 8 + 4*i), sizeof(uint32));
                result.push_back(j);
            }
            return result;
        }
        FilenameInfo getMWMOInfo(uint32 value)
        {
            // create Info
            FilenameInfo info;
            if (header.size)
            {
                memcpy(&info.offset, ((uint8*)this + 8 + 4*value), sizeof(info.offset));
                value++;
                if (value < getMaxWMO())
                {
                    uint32 nextoffset;
                    memcpy(&nextoffset, ((uint8*)this + 8 + 4*value), sizeof(uint32));
                    info.length = nextoffset - info.offset;
                }
            }
            return info;
        }
        uint32 getMaxWMO()
        {
            return header.size/sizeof(uint32);
        }
};

struct MDDF_Entry
{
    uint32 mmidEntry;
    uint32 uniqueId;
    float position[3];
    float rotation[3];
    uint16 scale;
    uint16 flags;
};

class adt_MDDF
{
        chunkHeader header;
    public:
        bool prepareLoadedData();
        uint32 getMaxEntries() {return header.size/sizeof(MDDF_Entry);}
};

//
// Adt file header chunk
//
class adt_MHDR
{
    chunkHeader header;

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


};

class ADT_file : public FileLoader{
public:
    bool prepareLoadedData();
    ADT_file();
    ~ADT_file();
    void free();

    adt_MHDR* a_grid;
};

#endif
