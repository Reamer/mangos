#ifndef WMO_H
#define WMO_H

#include "loadlib.h"
#include "../vec3d.h"

// MOPY flags
#define WMO_MATERIAL_NOCAMCOLLIDE    0x01
#define WMO_MATERIAL_DETAIL          0x02
#define WMO_MATERIAL_NO_COLLISION    0x04
#define WMO_MATERIAL_HINT            0x08
#define WMO_MATERIAL_RENDER          0x10
#define WMO_MATERIAL_COLLIDE_HIT     0x20
#define WMO_MATERIAL_WALL_SURFACE    0x40

class wmo_MOHD
{
    public:
        ChunkHeader header;
        uint32  nMaterials; // number of materials
        uint32  nGroups;    // number of WMO/v17 groups
        uint32  nPortals;   // number of portals
        uint32  nLights;    // number of lights
        uint32  nModels;    // number of M2 models imported
        uint32  nDoodads;   // number of dedicated files (*see below this table!)
        uint32  nDoodadSets;// number of doodad sets
        uint8  colR;
        uint8  colG;
        uint8  colB;
        uint8  colX;
        uint32  RootWMOID; //    WMO/v17 ID (column 2 in WMOAreaTable.dbc)
        float  boundingBoxCourner1[3];
        float  boundingBoxCourner2[3];
        uint32  liquidType;  // related, see below in the MLIQ chunk.
        bool prepareLoadedData();
};

class wmo_MOGP
{
    public:
        ChunkHeader header;
        uint32 groupName;       // Group name (offset into MOGN chunk)
        uint32 descGroupName;   //Descriptive group name (offset into MOGN chunk)
        uint32 mogpFlags;       // Flags
        float bbcorn1[3];      // Bounding box corner 1 (same as in MOGI)
        float bbcorn2[3];      // Bounding box corner 2
        uint16 moprIdx;         // Index into the MOPR chunk
        uint16 moprNItems;      // Number of items used from the MOPR chunk
        uint16 nBatchA;         // Number of batches A
        uint16 nBatchB;         // Number of batches B
        uint32 nBatchC;         // Number of batches C
        uint8  fogIdx[4];       // Up to four indices into the WMO fog list
        uint32 liquidType;      // LiquidType related, see below in the MLIQ chunk.
        uint32 groupWMOID;      // WMO group ID (column 4 in WMOAreaTable.dbc)
        bool prepareLoadedData();
};

struct wmo_MOPY_Entry
{
    uint8 flags;
    uint8 matrialId;
    wmo_MOPY_Entry() : flags(0), matrialId(0) {}
};
class wmo_MOPY
{
    public:
        ChunkHeader header;
        bool prepareLoadedData();
        inline uint32 getMaxEntries() { return header.size / sizeof(wmo_MOPY_Entry);}
        wmo_MOPY_Entry getMOPY_Entry(uint32 value);
};

struct wmo_MOVI_Entry
{
    uint16 vertex;
    wmo_MOVI_Entry() : vertex(0) {}
};
class wmo_MOVI
{
    public:
        ChunkHeader header;
        bool prepareLoadedData();
        inline uint32 getMaxEntries() { return header.size / sizeof(wmo_MOVI_Entry); }
        wmo_MOVI_Entry getMOVI_Entry(uint32 value);
};

struct wmo_MOVT_Entry
{
    float vertex[3];
    wmo_MOVT_Entry()
    {
        vertex[0] = 0.0f;
        vertex[1] = 0.0f;
        vertex[2] = 0.0f;
    }
};
class wmo_MOVT
{
    public:
        ChunkHeader header;
        bool prepareLoadedData();
        inline uint32 getMaxEntries() { return header.size / sizeof(wmo_MOVT_Entry); }
        wmo_MOVT_Entry getMOVT_Entry(uint32 value);
};

struct wmo_MOBA_Entry
{
    uint8 bytes[12];
    uint32 indexStart;
    uint16 indexCount, vertexStart, vertexEnd;
    uint8 flags, texture;
    wmo_MOBA_Entry() : indexStart(0), indexCount(0), vertexStart(0), vertexEnd(0),
                       flags(0), texture(0)
    {
        memset(bytes, 0, sizeof(uint8)*12);
    }
};
class wmo_MOBA
{
    public:
        ChunkHeader header;
        bool prepareLoadedData();
        inline uint32 getMaxEntries() { return header.size / sizeof(wmo_MOBA_Entry); }
        wmo_MOBA_Entry getMOBA_Entry(uint32 value);
};

struct WMOLiquidHeader
{
    uint32 xverts, yverts, xtiles, ytiles;
    float pos_x;
    float pos_y;
    float pos_z;
    uint16 type;
    WMOLiquidHeader() : xverts(0), yverts(0), xtiles(0), ytiles(0),
                        pos_x(0.0f), pos_y(0.0f), pos_z(0.0f),
                        type(0) {}
};

struct WMOLiquidVert
{
    uint16 unk1;
    uint16 unk2;
    float height;
    WMOLiquidVert() : unk1(0), unk2(0), height(0.0f)
    {}
};
class wmo_MLIQ
{
    public:
        ChunkHeader header;
        WMOLiquidHeader liquidHeader;
        bool prepareLoadedData();

        uint32 getLiqVertSize() { return liquidHeader.xverts * liquidHeader.yverts * sizeof(WMOLiquidVert);}
        uint32 getLiqVertMax() { return liquidHeader.xverts * liquidHeader.yverts;}
        WMOLiquidVert getLiquidVert(uint32 value);

        uint32 getLiquidBytesSize() {return liquidHeader.xtiles * liquidHeader.ytiles;}
        uint32 getLiquidBytesMax() {return getLiquidBytesSize(); }
        uint8 getLiquidBytes(uint32 value);

};

class WMORoot : public FileLoader
{
    public:
        WMORoot() {};
        ~WMORoot() { FileLoader::free(); };
        virtual bool prepareLoadedData();
        bool ConvertToVMAPRootWmo(FILE *output);
        wmo_MOHD* getMOHD() {return &m_wohd;}
    private:
        std::string filename;
        char outfilename;
        wmo_MOHD m_wohd;

};

class WMOGroup : public FileLoader
{
public:
    uint32 mopy_size,moba_size;
    uint32 nVertices; // number when loaded
    uint32 nTriangles; // number when loaded
    char *MOPY;
    uint16 *MOVI;
    uint16 *MoviEx;
    float *MOVT;
    uint16 *MOBA;
    int *MobaEx;
    WMOLiquidVert *LiquEx;
    char *LiquBytes;
    uint32 liquflags;

    WMOGroup() : mopy_size(0), moba_size(0), nVertices(0), nTriangles(0), liquflags(0) {};
    ~WMOGroup(){ FileLoader::free(); };
    virtual bool prepareLoadedData();
    int ConvertToVMAPGroupWmo(FILE *output, WMORoot *rootWMO, bool pPreciseVectorData);
    wmo_MOGP* getMOGP() { return &m_MOGP;};
    wmo_MOPY* getMOPY() { return &m_MOPY;};
    wmo_MOVI* getMOVI() { return &m_MOVI;};
    wmo_MOVT* getMOVT() { return &m_MOVT;};
    wmo_MOBA* getMOBA() { return &m_MOBA;};
    wmo_MLIQ* getMLIQ() { return &m_MLIQ;};


private:
    std::string filename;
    char outfilename;
    wmo_MOGP m_MOGP;
    wmo_MOPY m_MOPY;
    wmo_MOVI m_MOVI;
    wmo_MOVT m_MOVT;
    uint8* m_MONR;
    uint8* m_MOTV;
    wmo_MOBA m_MOBA;
    uint8* m_MOLR;
    uint8* m_MODR;
    uint8* m_MOBN;
    uint8* m_MOBR;
    uint8* m_MOCV;
    wmo_MLIQ m_MLIQ;
    uint8* m_unknown;
    uint8* m_MORI;
    uint8* m_MORB;
    uint8* m_MOTA;
    uint8* m_MOBS;
};

#endif
