#include <stdio.h>
#include <deque>
#include <map>
#include <set>
#include <cstdlib>
#include <string>

#ifdef WIN32
#include "direct.h"
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "dbcfile.h"

#include "loadlib/adt.h"
#include "loadlib/wdt.h"
#include "loadlib/wmo.h"
#include "loadlib/mdx_m2.h"
#include <fcntl.h>

// Map file format data
static char const* MAP_MAGIC         = "MAPS";
static char const* MAP_VERSION_MAGIC = "v1.2";
static char const* MAP_AREA_MAGIC    = "AREA";
static char const* MAP_HEIGHT_MAGIC  = "MHGT";
static char const* MAP_LIQUID_MAGIC  = "MLIQ";
static char const* szWorkDirWmo = "./Buildings";
static char const* szWorkDirMap = "./maps";
static char const* szWorkDirDBC = "./dbc";
static char const* RAW_VMAP_MAGIC = "VMAP004";
static char const* szWorkFileWmoAndM2 = "dir_bin";

struct GridMapFileHeader
{
    uint32 mapMagic;
    uint32 versionMagic;
    uint32 buildMagic;
    uint32 areaMapOffset;
    uint32 areaMapSize;
    uint32 heightMapOffset;
    uint32 heightMapSize;
    uint32 liquidMapOffset;
    uint32 liquidMapSize;
    uint32 holesOffset;
    uint32 holesSize;
};

#define MAP_AREA_NO_AREA      0x0001

struct GridMapAreaHeader
{
    uint32 fourcc;
    uint16 flags;
    uint16 gridArea;
};

#define MAP_HEIGHT_NO_HEIGHT  0x0001
#define MAP_HEIGHT_AS_INT16   0x0002
#define MAP_HEIGHT_AS_INT8    0x0004

struct GridMapHeightHeader
{
    uint32 fourcc;
    uint32 flags;
    float  gridHeight;
    float  gridMaxHeight;
};

#define MAP_LIQUID_TYPE_NO_WATER    0x00
#define MAP_LIQUID_TYPE_WATER       0x01
#define MAP_LIQUID_TYPE_OCEAN       0x02
#define MAP_LIQUID_TYPE_MAGMA       0x04
#define MAP_LIQUID_TYPE_SLIME       0x08
#define MAP_LIQUID_TYPE_DARK_WATER  0x10
#define MAP_LIQUID_TYPE_WMO_WATER   0x20


#define MAP_LIQUID_NO_TYPE    0x0001
#define MAP_LIQUID_NO_HEIGHT  0x0002

enum ModelFlags
{
    MOD_M2 = 1,
    MOD_WORLDSPAWN = 1<<1,
    MOD_HAS_BOUND = 1<<2
};

struct GridMapLiquidHeader
{
    uint32 fourcc;
    uint16 flags;
    uint16 liquidType;
    uint8  offsetX;
    uint8  offsetY;
    uint8  width;
    uint8  height;
    float  liquidLevel;
};

typedef struct
{
    char name[64];
    uint32 id;
} map_id;

void HandleArgs(int argc, char * arg[]);
void AppendDBCFileListTo(HANDLE mpqHandle, std::set<std::string>& filelist);
void AppendDB2FileListTo(HANDLE mpqHandle, std::set<std::string>& filelist);
void ReadBuild();
uint32 ReadMapDBC();
void ReadAreaTableDBC();
void ReadLiquidTypeTableDBC();
inline float selectUInt8StepStore(float maxDiff) { return 255 / maxDiff;}
inline float selectUInt16StepStore(float maxDiff) { return 65535 / maxDiff;}
bool ConvertADT(char* mpq_filename, char* output_filename, map_id mapid, int cell_y, int cell_x);
bool ParseBuildings(ADT_file* adt, char* mpq_filename, char* output_filename, map_id mapid, int cell_y, int cell_x);
bool ParseMap(ADT_file* adt, char* mpq_filename, char* output_filename, int cell_y, int cell_x);

bool ExtractWmo();
void ExtractGameobjectModels();
bool ExtractSingleModel(std::string fname);
bool ExtractSingleWmo(std::string fname);
void ExtractMapsFromMpq();
void ExtractDBCFiles();

// write Stuuf
bool writeModelInstance(MDDF_Entry mddf ,const char* ModelInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE *pDirfile);
bool writeWMOInstance(MODF_Entry modf,const char* WmoInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE *pDirfile);

// Load MPQ Files
void LoadLocaleMPQFiles();
void LoadCommonMPQFiles();


