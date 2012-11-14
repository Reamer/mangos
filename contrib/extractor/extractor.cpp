#define _CRT_SECURE_NO_DEPRECATE

#include "extractor.h"
#include "util.h"

map_id *map_ids;
uint16 *areas;
uint16 *LiqType;
char output_path[128] = ".";
char input_path[128] = ".";
uint32 maxAreaId = 0;
int32 build = 0;
int32 locale = -1;

//**************************************************
// Extractor options
//**************************************************
enum Extract
{
    EXTRACT_MAP = 0x1,
    EXTRACT_DBC = 0x2,
    EXTRACT_BUILDING = 0x4,
};

// Select data for extract
int   CONF_extract = EXTRACT_MAP | EXTRACT_DBC;
// This option allow limit minimum height to some value (Allow save some memory)
// see contrib/mmap/src/Tilebuilder.h, INVALID_MAP_LIQ_HEIGHT
bool  CONF_allow_height_limit = true;
float CONF_use_minHeight = -500.0f;


// This option allow use float to int conversion
bool  CONF_allow_float_to_int   = true;
float CONF_float_to_int8_limit  = 2.0f;      // Max accuracy = val/256
float CONF_float_to_int16_limit = 2048.0f;   // Max accuracy = val/65536
float CONF_flat_height_delta_limit = 0.005f; // If max - min less this value - surface is flat
float CONF_flat_liquid_delta_limit = 0.001f; // If max - min less this value - liquid surface is flat

// List MPQ for extract from
const char *CONF_mpq_list[]={
    "common.MPQ",
    "common-2.MPQ",
    "lichking.MPQ",
    "expansion.MPQ",
    "patch.MPQ",
    "patch-2.MPQ",
    "patch-3.MPQ",
    "patch-4.MPQ",
    "patch-5.MPQ",
};

static const char* const langs[] = {"enGB", "enUS", "deDE", "esES", "frFR", "koKR", "zhCN", "zhTW", "enCN", "enTW", "esMX", "ruRU" };

void Usage(char* prg)
{
    printf(
        "Usage:\n"\
        "%s -[var] [value]\n"\
        "-i set input path\n"\
        "-o set output path\n"\
        "-e extract only MAP(1)/DBC(2) - standard: both(3)\n"\
        "-f height stored as int (less map size but lost some accuracy) 1 by default\n"\
        "Example: %s -f 0 -i \"c:\\games\\game\"", prg, prg);
    exit(1);
}

void HandleArgs(int argc, char * arg[])
{
    for(int c = 1; c < argc; ++c)
    {
        // i - input path
        // o - output path
        // e - extract only MAP(1)/DBC(2) - standard both(3)
        // f - use float to int conversion
        // h - limit minimum height
        if(arg[c][0] != '-')
            Usage(arg[0]);

        switch(arg[c][1])
        {
            case 'i':
                if(c + 1 < argc)                            // all ok
                    strcpy(input_path, arg[(c++) + 1]);
                else
                    Usage(arg[0]);
                break;
            case 'o':
                if(c + 1 < argc)                            // all ok
                    strcpy(output_path, arg[(c++) + 1]);
                else
                    Usage(arg[0]);
                break;
            case 'f':
                if(c + 1 < argc)                            // all ok
                    CONF_allow_float_to_int=atoi(arg[(c++) + 1])!=0;
                else
                    Usage(arg[0]);
                break;
            case 'e':
                if(c + 1 < argc)                            // all ok
                {
                    CONF_extract=atoi(arg[(c++) + 1]);
                    if(!(CONF_extract > 0 && CONF_extract < 4))
                        Usage(arg[0]);
                }
                else
                    Usage(arg[0]);
                break;
            default:
                break;
        }
    }
}

void AppendDBCFileListTo(HANDLE mpqHandle, std::set<std::string>& filelist)
{
    SFILE_FIND_DATA findFileData;

    HANDLE searchHandle = SFileFindFirstFile(mpqHandle, "*.dbc", &findFileData, NULL);
    if (!searchHandle)
        return;

    filelist.insert(findFileData.cFileName);

    while (SFileFindNextFile(searchHandle, &findFileData))
        filelist.insert(findFileData.cFileName);

    SFileFindClose(searchHandle);
}

// for Cata we need DB2 files
void AppendDB2FileListTo(HANDLE mpqHandle, std::set<std::string>& filelist)
{
    SFILE_FIND_DATA findFileData;

    HANDLE searchHandle = SFileFindFirstFile(mpqHandle, "*.db2", &findFileData, NULL);
    if (!searchHandle)
        return;

    filelist.insert(findFileData.cFileName);

    while (SFileFindNextFile(searchHandle, &findFileData))
        filelist.insert(findFileData.cFileName);

    SFileFindClose(searchHandle);
}

void ReadBuild()
{
    // include build info file also
    std::string filename  = std::string("component.wow-")+langs[locale]+".txt";
    //printf("Read %s file... ", filename.c_str());

    HANDLE fileHandle;

    if (!OpenNewestFile(filename.c_str(), &fileHandle))
    {
        printf("Fatal error: Not found %s file!\n", filename.c_str());
        exit(1);
    }

    unsigned int data_size = SFileGetFileSize(fileHandle, NULL);

    std::string text;
    text.resize(data_size);

    if (!SFileReadFile(fileHandle, &text[0], data_size, NULL, NULL))
    {
        printf("Fatal error: Can't read %s file!\n", filename.c_str());
        exit(1);
    }

    SFileCloseFile(fileHandle);

    size_t pos = text.find("version=\"");
    size_t pos1 = pos + strlen("version=\"");
    size_t pos2 = text.find("\"",pos1);
    if (pos == text.npos || pos2 == text.npos || pos1 >= pos2)
    {
        printf("Fatal error: Invalid  %s file format!\n", filename.c_str());
        exit(1);
    }

    std::string build_str = text.substr(pos1,pos2-pos1);

    build = atoi(build_str.c_str());
    if (build <= 0)
    {
        printf("Fatal error: Invalid  %s file format!\n", filename.c_str());
        exit(1);
    }
}

uint32 ReadMapDBC()
{
    printf("Read Map.dbc file... ");
    DBCFile dbc("DBFilesClient\\Map.dbc");

    if(!dbc.open())
    {
        printf("Fatal error: Invalid Map.dbc file format!\n");
        exit(1);
    }

    size_t map_count = dbc.getRecordCount();
    map_ids = new map_id[map_count];
    for(uint32 x = 0; x < map_count; ++x)
    {
        map_ids[x].id = dbc.getRecord(x).getUInt(0);
        strcpy(map_ids[x].name, dbc.getRecord(x).getString(1));
    }
    printf("Done! (%zu maps loaded)\n", map_count);
    return map_count;
}

void ReadAreaTableDBC()
{
    printf("Read AreaTable.dbc file...");
    DBCFile dbc("DBFilesClient\\AreaTable.dbc");

    if(!dbc.open())
    {
        printf("Fatal error: Invalid AreaTable.dbc file format!\n");
        exit(1);
    }

    size_t area_count = dbc.getRecordCount();
    size_t maxid = dbc.getMaxId();
    areas = new uint16[maxid + 1];
    memset(areas, 0xff, (maxid + 1) * sizeof(uint16));

    for(uint32 x = 0; x < area_count; ++x)
        areas[dbc.getRecord(x).getUInt(0)] = dbc.getRecord(x).getUInt(3);

    maxAreaId = dbc.getMaxId();

    printf("Done! (%zu areas loaded)\n", area_count);
}

void ReadLiquidTypeTableDBC()
{
    printf("Read LiquidType.dbc file...");
    DBCFile dbc("DBFilesClient\\LiquidType.dbc");
    if(!dbc.open())
    {
        printf("Fatal error: Invalid LiquidType.dbc file format!\n");
        exit(1);
    }

    size_t LiqType_count = dbc.getRecordCount();
    size_t LiqType_maxid = dbc.getMaxId();
    LiqType = new uint16[LiqType_maxid + 1];
    memset(LiqType, 0xff, (LiqType_maxid + 1) * sizeof(uint16));

    for(uint32 x = 0; x < LiqType_count; ++x)
        LiqType[dbc.getRecord(x).getUInt(0)] = dbc.getRecord(x).getUInt(3);

    printf("Done! (%zu LiqTypes loaded)\n", LiqType_count);
}

bool ParseBuildings(ADT_file* adt, char* mpq_filename, char* output_filename, map_id mapid , int cell_y, int cell_x)
{
    std::string dirname = std::string(szWorkDirWmo) + "/" + std::string(szWorkFileWmoAndM2);
    FILE *dirfile;
    dirfile = fopen(dirname.c_str(), "ab");
    if(!dirfile)
    {
        printf("Can't open dirfile!'%s'\n", dirname.c_str());
        return false;
    }

    adt_MODF* modf = adt->getMHDR()->getMODF();
    adt_MWMO* mwmo = adt->getMHDR()->getMWMO();
    adt_MWID* mwid = adt->getMHDR()->getMWID();
    if (modf && mwmo && mwid)
    {
        for (uint32 i = 0; i < modf->getMaxEntries(); ++i)
        {
            MODF_Entry entry = modf->getMODF_Entry(i);
            FilenameInfo info = mwid->getMWMOInfo(&entry);
            mwmo->getWMOFilename(&info);
            //changeWhitespaceToUnderscore(info.filename);
            transformToPath(info.filename);
            //printf("Filename: %s\n", info.filename.c_str());
            ExtractSingleWmo(info.filename);
            writeWMOInstance(entry, GetPlainName(info.filename.c_str()), mapid.id, cell_x, cell_y, dirfile);
        }
    }

    adt_MMDX* mmdx = adt->getMHDR()->getMMDX();
    adt_MDDF* mddf = adt->getMHDR()->getMDDF();
    adt_MMID* mmid = adt->getMHDR()->getMMID();
    if (mmdx && mddf && mmid)
    {
        for (uint32 i = 0; i < mddf->getMaxEntries(); ++i)
        {
            MDDF_Entry entry = mddf->getMDDF_Entry(i);
            FilenameInfo info = mmid->getMMDXInfo(&entry);
            mmdx->getM2Model(&info);
            //changeWhitespaceToUnderscore(info.filename);
            transformToPath(info.filename);
            changeMDXtoM2(info.filename);
            ExtractSingleModel(info.filename);
            writeModelInstance(entry, GetPlainName(info.filename.c_str()), mapid.id, cell_x, cell_y, dirfile);

        }
    }
    fclose(dirfile);
    return true;
}

bool writeModelInstance(MDDF_Entry mddf ,const char* ModelInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE *pDirfile)
{
    Vec3D pos = fixCoords(Vec3D(mddf.position[0],mddf.position[1],mddf.position[2]));
    Vec3D rot = Vec3D(mddf.rotation[0],mddf.rotation[1],mddf.rotation[2]);
    // scale factor - divide by 1024. blizzard devs must be on crack, why not just use a float?
    float sc = mddf.scale / 1024.0f;

    char tempname[512];
    sprintf(tempname, "%s/%s", szWorkDirWmo, ModelInstName);
    FILE *input;
    input = fopen(tempname, "r+b");

    if(!input)
    {
        //printf("writeModelInstance couldn't open %s\n", tempname);
        return false;
    }

    fseek(input, 8, SEEK_SET); // get the correct no of vertices
    int nVertices;
    fread(&nVertices, sizeof (int), 1, input);
    fclose(input);

    if(nVertices == 0)
    {
        //printf("ERROR: no Vertices\n");
        return false;
    }

    uint16 adtId = 0;// not used for models
    uint32 flags = MOD_M2;
    if(tileX == 65 && tileY == 65) flags |= MOD_WORLDSPAWN;
    //write mapID, tileX, tileY, Flags, ID, Pos, Rot, Scale, name
    fwrite(&mapID, sizeof(uint32), 1, pDirfile);
    fwrite(&tileX, sizeof(uint32), 1, pDirfile);
    fwrite(&tileY, sizeof(uint32), 1, pDirfile);
    fwrite(&flags, sizeof(uint32), 1, pDirfile);
    fwrite(&adtId, sizeof(uint16), 1, pDirfile);
    fwrite(&mddf.uniqueId, sizeof(uint32), 1, pDirfile);
    fwrite(&pos, sizeof(float), 3, pDirfile);
    fwrite(&rot, sizeof(float), 3, pDirfile);
    fwrite(&sc, sizeof(float), 1, pDirfile);
    uint32 nlen=strlen(ModelInstName);
    fwrite(&nlen, sizeof(uint32), 1, pDirfile);
    fwrite(ModelInstName, sizeof(char), nlen, pDirfile);

    /* int realx1 = (int) ((float) pos.x / 533.333333f);
    int realy1 = (int) ((float) pos.z / 533.333333f);
    int realx2 = (int) ((float) pos.x / 533.333333f);
    int realy2 = (int) ((float) pos.z / 533.333333f);

    fprintf(pDirfile,"%s/%s %f,%f,%f_%f,%f,%f %f %d %d %d,%d %d\n",
        MapName,
        ModelInstName,
        (float) pos.x, (float) pos.y, (float) pos.z,
        (float) rot.x, (float) rot.y, (float) rot.z,
        sc,
        nVertices,
        realx1, realy1,
        realx2, realy2
        ); */

    return true;
}

bool writeWMOInstance(MODF_Entry modf,const char* WmoInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE *pDirfile)
{
    Vec3D pos = Vec3D(modf.position[0],modf.position[1],modf.position[2]);
    Vec3D rot = Vec3D(modf.rotation[0],modf.rotation[1],modf.rotation[2]);
    Vec3D pos2 = Vec3D(modf.upperExtents[0],modf.upperExtents[1],modf.upperExtents[2]);
    Vec3D pos3 = Vec3D(modf.lowerExtents[0],modf.lowerExtents[1],modf.lowerExtents[2]);

    uint16 adtId = modf.nameSet;

    //-----------add_in _dir_file----------------

    char tempname[512];
    sprintf(tempname, "%s/%s", szWorkDirWmo, WmoInstName);
    FILE *input;
    input = fopen(tempname, "r+b");

    if(!input)
    {
        printf("writeWMOInstance: couldn't open %s\n", tempname);
        return false;
    }

    fseek(input, 8, SEEK_SET); // get the correct no of vertices
    int nVertices;
    fread(&nVertices, sizeof (int), 1, input);
    fclose(input);

    if(nVertices == 0)
        return false;

    float x,z;
    x = pos.x;
    z = pos.z;
    if(x==0 && z == 0)
    {
        pos.x = 533.33333f*32;
        pos.z = 533.33333f*32;
    }
    pos = fixCoords(pos);
    pos2 = fixCoords(pos2);
    pos3 = fixCoords(pos3);

    float scale = 1.0f;
    uint32 flags = MOD_HAS_BOUND;
    if(tileX == 65 && tileY == 65) flags |= MOD_WORLDSPAWN;
    //write mapID, tileX, tileY, Flags, ID, Pos, Rot, Scale, Bound_lo, Bound_hi, name
    fwrite(&mapID, sizeof(uint32), 1, pDirfile);
    fwrite(&tileX, sizeof(uint32), 1, pDirfile);
    fwrite(&tileY, sizeof(uint32), 1, pDirfile);
    fwrite(&flags, sizeof(uint32), 1, pDirfile);
    fwrite(&adtId, sizeof(uint16), 1, pDirfile);
    fwrite(&modf.uniqueId, sizeof(uint32), 1, pDirfile);
    fwrite(&pos, sizeof(float), 3, pDirfile);
    fwrite(&rot, sizeof(float), 3, pDirfile);
    fwrite(&scale, sizeof(float), 1, pDirfile);
    fwrite(&pos2, sizeof(float), 3, pDirfile);
    fwrite(&pos3, sizeof(float), 3, pDirfile);
    uint32 nlen=strlen(WmoInstName);
    fwrite(&nlen, sizeof(uint32), 1, pDirfile);
    fwrite(WmoInstName, sizeof(char), nlen, pDirfile);

    /* fprintf(pDirfile,"%s/%s %f,%f,%f_%f,%f,%f 1.0 %d %d %d,%d %d\n",
        MapName,
        WmoInstName,
        (float) x, (float) pos.y, (float) z,
        (float) rot.x, (float) rot.y, (float) rot.z,
        nVertices,
        realx1, realy1,
        realx2, realy2
        ); */

    // fclose(dirfile);
    return true;
}

bool ExtractSingleModel(std::string fname)
{
    changeMDXtoM2(fname);

    std::string output(szWorkDirWmo);
    output += "/";
    output += GetPlainName(fname.c_str());

    if (FileExists(output.c_str()))
        return true;

    MDX_M2_file model;
    if (!model.loadFile(fname.c_str()))
    {
        printf("ERROR: Load MDX_M2_File failed: %s\n", fname.c_str());
        return false;
    }

    return model.ConvertToVMAPModel(output.c_str());
}

void ExtractGameobjectModels()
{
    printf("Extracting GameObject models...\n");
    DBCFile dbc("DBFilesClient\\GameObjectDisplayInfo.dbc");
    if(!dbc.open())
    {
        printf("Fatal error: Invalid GameObjectDisplayInfo.dbc file format!\n");
        exit(1);
    }

    std::string basepath = szWorkDirWmo;
    basepath += "/";
    std::string path;

    FILE * model_list = fopen((basepath + "temp_gameobject_models").c_str(), "wb");

    for (DBCFile::Iterator it = dbc.begin(); it != dbc.end(); ++it)
    {
        path = it->getString(1);

        if (path.length() < 4)
            continue;

        transformToPath(path);
        //changeWhitespaceToUnderscore(path);

        char const* name = GetPlainName(path.c_str());

        char const* ch_ext = GetExtension(name);
        if (!ch_ext)
            continue;

        bool result = false;
        if (!strcmp(ch_ext, ".wmo"))
        {
            result = ExtractSingleWmo(path);
        }
        else if (!strcmp(ch_ext, ".mdl"))
        {
            // TODO: extract .mdl files, if needed
            continue;
        }
        else //if (!strcmp(ch_ext, ".mdx") || !strcmp(ch_ext, ".m2"))
        {
            transformToPath(path);
            changeMDXtoM2(path);
            result = ExtractSingleModel(path);
        }

        if (result)
        {
            uint32 displayId = it->getUInt(0);
            uint32 path_length = strlen(name);
            fwrite(&displayId, sizeof(uint32), 1, model_list);
            fwrite(&path_length, sizeof(uint32), 1, model_list);
            fwrite(name, sizeof(char), path_length, model_list);
        }
    }

    fclose(model_list);

    printf("Done!\n");
}


bool ExtractSingleWmo(std::string fname)
{
    // Copy files from archive
    char szLocalFile[1024];
    const char * plain_name = GetPlainName(fname.c_str());
    sprintf(szLocalFile, "%s/%s", szWorkDirWmo, plain_name);
    transformToPath(szLocalFile,strlen(szLocalFile));

    if (FileExists(szLocalFile))
        return true;

    int p = 0;
    //Select root wmo files
    const char * rchr = strrchr(plain_name, '_');
    if(rchr != NULL)
    {
        char cpy[4];
        strncpy(cpy,rchr,4);
        for (int i=0;i < 4; ++i)
        {
            int m = cpy[i];
            if(isdigit(m))
                p++;
        }
    }

    if (p == 3)
        return true;

    bool file_ok = true;
//    printf("Extracting %s\n", fname.c_str());
    WMORoot froot;
    if(!froot.loadFile(fname.c_str()))
    {
        printf("ExtractSingleWmo: Couldn't open RootWmo!!!\n");
        return true;
    }
    FILE *output = fopen(szLocalFile,"wb");
    if(!output)
    {
        printf("ExtractSingleWmo: couldn't open %s for writing!\n", szLocalFile);
        return false;
    }
    froot.ConvertToVMAPRootWmo(output);
    int Wmo_nVertices = 0;
    //printf("root has %d groups\n", froot->nGroups);
    if (froot.getMOHD()->nGroups !=0)
    {
        for (uint32 i = 0; i < froot.getMOHD()->nGroups; ++i)
        {
            char temp[1024];
            strcpy(temp, fname.c_str());
            temp[fname.length()-4] = 0;
            char groupFileName[1024];
            sprintf(groupFileName,"%s_%03d.wmo",temp, i);
            //printf("Trying to open groupfile %s\n",groupFileName);

            std::string s = groupFileName;
            WMOGroup fgroup;
            if(!fgroup.loadFile(s.c_str()))
            {
                printf("Could not open all Group file for: %s\n", plain_name);
                file_ok = false;
                break;
            }
            bool preciseVectorData =  false;
            Wmo_nVertices += fgroup.ConvertToVMAPGroupWmo(output, &froot, preciseVectorData);
        }
    }

    fseek(output, 8, SEEK_SET); // store the correct no of vertices
    fwrite(&Wmo_nVertices,sizeof(int),1,output);
    fclose(output);

    // Delete the extracted file in the case of an error
    if (!file_ok)
        remove(szLocalFile);
    return true;
}


bool ParseMap(ADT_file* adt, char* mpq_filename, char* output_filename, int cell_y, int cell_x)
{
    adt_MCIN *cells = adt->getMHDR()->getMCIN();
    if (!cells)
    {
        printf("Can't find cells in '%s'\n", mpq_filename);
        return false;
    }

    // Temporary grid data store
    uint16 area_flags[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];

    float V8[ADT_GRID_SIZE][ADT_GRID_SIZE];
    float V9[ADT_GRID_SIZE+1][ADT_GRID_SIZE+1];
    uint16 uint16_V8[ADT_GRID_SIZE][ADT_GRID_SIZE];
    uint16 uint16_V9[ADT_GRID_SIZE+1][ADT_GRID_SIZE+1];
    uint8  uint8_V8[ADT_GRID_SIZE][ADT_GRID_SIZE];
    uint8  uint8_V9[ADT_GRID_SIZE+1][ADT_GRID_SIZE+1];

    uint16 liquid_entry[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];
    uint8 liquid_flags[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];
    bool  liquid_show[ADT_GRID_SIZE][ADT_GRID_SIZE];
    float liquid_height[ADT_GRID_SIZE+1][ADT_GRID_SIZE+1];


    memset(liquid_show, 0, sizeof(liquid_show));
    memset(liquid_flags, 0, sizeof(liquid_flags));
    memset(liquid_entry, 0, sizeof(liquid_entry));

    // Prepare map header
    GridMapFileHeader map;
    map.mapMagic = *(uint32 const*)MAP_MAGIC;
    map.versionMagic = *(uint32 const*)MAP_VERSION_MAGIC;
    map.buildMagic = build;

    // Get area flags data
    for (int i=0;i<ADT_CELLS_PER_GRID;i++)
    {
        for(int j=0;j<ADT_CELLS_PER_GRID;j++)
        {
            adt_MCNK * cell = cells->getMCNK(i,j);
            uint32 areaid = cell->areaid;
            if(areaid && areaid <= maxAreaId)
            {
                if(areas[areaid] != 0xffff)
                {
                    area_flags[i][j] = areas[areaid];
                    continue;
                }
                printf("File: %s\nCan't find area flag for areaid %u [%d, %d].\n", mpq_filename, areaid, cell->ix, cell->iy);
            }
            area_flags[i][j] = 0xffff;
        }
    }
    //============================================
    // Try pack area data
    //============================================
    bool fullAreaData = false;
    uint32 areaflag = area_flags[0][0];
    for (int y=0;y<ADT_CELLS_PER_GRID;y++)
    {
        for(int x=0;x<ADT_CELLS_PER_GRID;x++)
        {
            if(area_flags[y][x]!=areaflag)
            {
                fullAreaData = true;
                break;
            }
        }
    }

    map.areaMapOffset = sizeof(map);
    map.areaMapSize   = sizeof(GridMapAreaHeader);

    GridMapAreaHeader areaHeader;
    areaHeader.fourcc = *(uint32 const*)MAP_AREA_MAGIC;
    areaHeader.flags = 0;
    if (fullAreaData)
    {
        areaHeader.gridArea = 0;
        map.areaMapSize+=sizeof(area_flags);
    }
    else
    {
        areaHeader.flags |= MAP_AREA_NO_AREA;
        areaHeader.gridArea = (uint16)areaflag;
    }

    //
    // Get Height map from grid
    //
    for (int i=0;i<ADT_CELLS_PER_GRID;i++)
    {
        for(int j=0;j<ADT_CELLS_PER_GRID;j++)
        {
            adt_MCNK * cell = cells->getMCNK(i,j);
            if (!cell)
                continue;
            // Height values for triangles stored in order:
            // 1     2     3     4     5     6     7     8     9
            //    10    11    12    13    14    15    16    17
            // 18    19    20    21    22    23    24    25    26
            //    27    28    29    30    31    32    33    34
            // . . . . . . . .
            // For better get height values merge it to V9 and V8 map
            // V9 height map:
            // 1     2     3     4     5     6     7     8     9
            // 18    19    20    21    22    23    24    25    26
            // . . . . . . . .
            // V8 height map:
            //    10    11    12    13    14    15    16    17
            //    27    28    29    30    31    32    33    34
            // . . . . . . . .

            // Set map height as grid height
            for (int y=0; y <= ADT_CELL_SIZE; y++)
            {
                int cy = i*ADT_CELL_SIZE + y;
                for (int x=0; x <= ADT_CELL_SIZE; x++)
                {
                    int cx = j*ADT_CELL_SIZE + x;
                    V9[cy][cx]=cell->ypos;
                }
            }
            for (int y=0; y < ADT_CELL_SIZE; y++)
            {
                int cy = i*ADT_CELL_SIZE + y;
                for (int x=0; x < ADT_CELL_SIZE; x++)
                {
                    int cx = j*ADT_CELL_SIZE + x;
                    V8[cy][cx]=cell->ypos;
                }
            }
            // Get custom height
            adt_MCVT *v = cell->getMCVT();
            if (!v)
                continue;
            // get V9 height map
            for (int y=0; y <= ADT_CELL_SIZE; y++)
            {
                int cy = i*ADT_CELL_SIZE + y;
                for (int x=0; x <= ADT_CELL_SIZE; x++)
                {
                    int cx = j*ADT_CELL_SIZE + x;
                    V9[cy][cx]+=v->height_map[y*(ADT_CELL_SIZE*2+1)+x];
                }
            }
            // get V8 height map
            for (int y=0; y < ADT_CELL_SIZE; y++)
            {
                int cy = i*ADT_CELL_SIZE + y;
                for (int x=0; x < ADT_CELL_SIZE; x++)
                {
                    int cx = j*ADT_CELL_SIZE + x;
                    V8[cy][cx]+=v->height_map[y*(ADT_CELL_SIZE*2+1)+ADT_CELL_SIZE+1+x];
                }
            }
        }
    }
    //============================================
    // Try pack height data
    //============================================
    float maxHeight = -20000;
    float minHeight =  20000;
    for (int y=0; y<ADT_GRID_SIZE; y++)
    {
        for(int x=0;x<ADT_GRID_SIZE;x++)
        {
            float h = V8[y][x];
            if (maxHeight < h) maxHeight = h;
            if (minHeight > h) minHeight = h;
        }
    }
    for (int y=0; y<=ADT_GRID_SIZE; y++)
    {
        for(int x=0;x<=ADT_GRID_SIZE;x++)
        {
            float h = V9[y][x];
            if (maxHeight < h) maxHeight = h;
            if (minHeight > h) minHeight = h;
        }
    }

    // Check for allow limit minimum height (not store height in deep ochean - allow save some memory)
    if (CONF_allow_height_limit && minHeight < CONF_use_minHeight)
    {
        for (int y=0; y<ADT_GRID_SIZE; y++)
            for(int x=0;x<ADT_GRID_SIZE;x++)
                if (V8[y][x] < CONF_use_minHeight)
                    V8[y][x] = CONF_use_minHeight;
        for (int y=0; y<=ADT_GRID_SIZE; y++)
            for(int x=0;x<=ADT_GRID_SIZE;x++)
                if (V9[y][x] < CONF_use_minHeight)
                    V9[y][x] = CONF_use_minHeight;
        if (minHeight < CONF_use_minHeight)
            minHeight = CONF_use_minHeight;
        if (maxHeight < CONF_use_minHeight)
            maxHeight = CONF_use_minHeight;
    }

    map.heightMapOffset = map.areaMapOffset + map.areaMapSize;
    map.heightMapSize = sizeof(GridMapHeightHeader);

    GridMapHeightHeader heightHeader;
    heightHeader.fourcc = *(uint32 const*)MAP_HEIGHT_MAGIC;
    heightHeader.flags = 0;
    heightHeader.gridHeight    = minHeight;
    heightHeader.gridMaxHeight = maxHeight;

    if (maxHeight == minHeight)
        heightHeader.flags |= MAP_HEIGHT_NO_HEIGHT;

    // Not need store if flat surface
    if (CONF_allow_float_to_int && (maxHeight - minHeight) < CONF_flat_height_delta_limit)
        heightHeader.flags |= MAP_HEIGHT_NO_HEIGHT;

    // Try store as packed in uint16 or uint8 values
    if (!(heightHeader.flags & MAP_HEIGHT_NO_HEIGHT))
    {
        float step;
        // Try Store as uint values
        if (CONF_allow_float_to_int)
        {
            float diff = maxHeight - minHeight;
            if (diff < CONF_float_to_int8_limit)      // As uint8 (max accuracy = CONF_float_to_int8_limit/256)
            {
                heightHeader.flags|=MAP_HEIGHT_AS_INT8;
                step = selectUInt8StepStore(diff);
            }
            else if (diff<CONF_float_to_int16_limit)  // As uint16 (max accuracy = CONF_float_to_int16_limit/65536)
            {
                heightHeader.flags|=MAP_HEIGHT_AS_INT16;
                step = selectUInt16StepStore(diff);
            }
        }

        // Pack it to int values if need
        if (heightHeader.flags&MAP_HEIGHT_AS_INT8)
        {
            for (int y=0; y<ADT_GRID_SIZE; y++)
                for(int x=0;x<ADT_GRID_SIZE;x++)
                    uint8_V8[y][x] = uint8((V8[y][x] - minHeight) * step + 0.5f);
            for (int y=0; y<=ADT_GRID_SIZE; y++)
                for(int x=0;x<=ADT_GRID_SIZE;x++)
                    uint8_V9[y][x] = uint8((V9[y][x] - minHeight) * step + 0.5f);
            map.heightMapSize+= sizeof(uint8_V9) + sizeof(uint8_V8);
        }
        else if (heightHeader.flags&MAP_HEIGHT_AS_INT16)
        {
            for (int y=0; y<ADT_GRID_SIZE; y++)
                for(int x=0;x<ADT_GRID_SIZE;x++)
                    uint16_V8[y][x] = uint16((V8[y][x] - minHeight) * step + 0.5f);
            for (int y=0; y<=ADT_GRID_SIZE; y++)
                for(int x=0;x<=ADT_GRID_SIZE;x++)
                    uint16_V9[y][x] = uint16((V9[y][x] - minHeight) * step + 0.5f);
            map.heightMapSize+= sizeof(uint16_V9) + sizeof(uint16_V8);
        }
        else
            map.heightMapSize+= sizeof(V9) + sizeof(V8);
    }

    // Get from MCLQ chunk (old)
    for (int i = 0; i < ADT_CELLS_PER_GRID; i++)
    {
        for(int j = 0; j < ADT_CELLS_PER_GRID; j++)
        {
            adt_MCNK *cell = cells->getMCNK(i, j);
            if (!cell)
                continue;

            adt_MCLQ *liquid = cell->getMCLQ();
            int count = 0;
            if (!liquid || cell->sizeMCLQ <= 8)
                continue;

            for (int y = 0; y < ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x < ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    if (liquid->flags[y][x] != 0x0F)
                    {
                        liquid_show[cy][cx] = true;
                        if (liquid->flags[y][x] & (1<<7))
                            liquid_flags[i][j] |= MAP_LIQUID_TYPE_DARK_WATER;
                        ++count;
                    }
                }
            }

            uint32 c_flag = cell->flags;
            if (c_flag & (1<<2))
            {
                liquid_entry[i][j] = 1;
                liquid_flags[i][j] |= MAP_LIQUID_TYPE_WATER;            // water
            }
            if (c_flag & (1<<3))
            {
                liquid_entry[i][j] = 2;
                liquid_flags[i][j] |= MAP_LIQUID_TYPE_OCEAN;            // ocean
            }
            if (c_flag & (1<<4))
            {
                liquid_entry[i][j] = 3;
                liquid_flags[i][j] |= MAP_LIQUID_TYPE_MAGMA;            // magma/slime
            }

            if (!count && liquid_flags[i][j])
                fprintf(stderr, "Wrong liquid detect in MCLQ chunk");

            for (int y = 0; y <= ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x <= ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    liquid_height[cy][cx] = liquid->liquid[y][x].height;
                }
            }
        }
    }

    // Get liquid map for grid (in WOTLK used MH2O chunk)
    adt_MH2O * h2o = adt->getMHDR()->getMH2O();
    if (h2o)
    {
        for (int i = 0; i < ADT_CELLS_PER_GRID; i++)
        {
            for(int j = 0; j < ADT_CELLS_PER_GRID; j++)
            {
                adt_liquid_header* liquid_header = h2o->getLiquidData(i,j);
                if (!liquid_header)
                    continue;

                int count = 0;
                uint64 show = h2o->getLiquidShowMap(liquid_header);
                for (int y = 0; y < liquid_header->height; y++)
                {
                    int cy = i * ADT_CELL_SIZE + y + liquid_header->yOffset;
                    for (int x = 0; x < liquid_header->width; x++)
                    {
                        int cx = j * ADT_CELL_SIZE + x + liquid_header->xOffset;
                        if (show & 1)
                        {
                            liquid_show[cy][cx] = true;
                            ++count;
                        }
                        show >>= 1;
                    }
                }

                liquid_entry[i][j] = liquid_header->liquidType;
                switch (LiqType[liquid_header->liquidType])
                {
                    case LIQUID_TYPE_WATER: liquid_flags[i][j] |= MAP_LIQUID_TYPE_WATER; break;
                    case LIQUID_TYPE_OCEAN: liquid_flags[i][j] |= MAP_LIQUID_TYPE_OCEAN; break;
                    case LIQUID_TYPE_MAGMA: liquid_flags[i][j] |= MAP_LIQUID_TYPE_MAGMA; break;
                    case LIQUID_TYPE_SLIME: liquid_flags[i][j] |= MAP_LIQUID_TYPE_SLIME; break;
                    default:
                        printf("\nCan't find Liquid type %u for map %s\nchunk %d,%d\n", liquid_header->liquidType, mpq_filename, i, j);
                        break;
                }
                // Dark water detect
                if (LiqType[liquid_header->liquidType] == LIQUID_TYPE_OCEAN)
                {
                    uint8 *lm = h2o->getLiquidLightMap(liquid_header);
                    if (!lm)
                        liquid_flags[i][j] |= MAP_LIQUID_TYPE_DARK_WATER;
                }

                if (!count && liquid_flags[i][j])
                    printf("Wrong liquid detect in MH2O chunk");

                float *height = h2o->getLiquidHeightMap(liquid_header);
                int pos = 0;
                for (int y=0; y<=liquid_header->height;y++)
                {
                    int cy = i*ADT_CELL_SIZE + y + liquid_header->yOffset;
                    for (int x=0; x<= liquid_header->width; x++)
                    {
                        int cx = j*ADT_CELL_SIZE + x + liquid_header->xOffset;
                        if (height)
                            liquid_height[cy][cx] = height[pos];
                        else
                            liquid_height[cy][cx] = liquid_header->heightLevel1;
                        pos++;
                    }
                }
            }
        }
    }
    //============================================
    // Pack liquid data
    //============================================
    uint8 type = liquid_flags[0][0];
    bool fullType = false;
    for (int y=0;y<ADT_CELLS_PER_GRID;y++)
    {
        for(int x=0;x<ADT_CELLS_PER_GRID;x++)
        {
            if (liquid_flags[y][x]!=type)
            {
                fullType = true;
                y = ADT_CELLS_PER_GRID;
                break;
            }
        }
    }

    GridMapLiquidHeader liquidHeader;

    // no water data (if all grid have 0 liquid type)
    if (type == 0 && !fullType)
    {
        // No liquid data
        map.liquidMapOffset = 0;
        map.liquidMapSize   = 0;
    }
    else
    {
        int minX = 255, minY = 255;
        int maxX = 0, maxY = 0;
        maxHeight = -20000;
        minHeight = 20000;
        for (int y=0; y<ADT_GRID_SIZE; y++)
        {
            for(int x=0; x<ADT_GRID_SIZE; x++)
            {
                if (liquid_show[y][x])
                {
                    if (minX > x) minX = x;
                    if (maxX < x) maxX = x;
                    if (minY > y) minY = y;
                    if (maxY < y) maxY = y;
                    float h = liquid_height[y][x];
                    if (maxHeight < h) maxHeight = h;
                    if (minHeight > h) minHeight = h;
                }
                else
                    liquid_height[y][x] = CONF_use_minHeight;
            }
        }
        map.liquidMapOffset = map.heightMapOffset + map.heightMapSize;
        map.liquidMapSize = sizeof(GridMapLiquidHeader);
        liquidHeader.fourcc = *(uint32 const*)MAP_LIQUID_MAGIC;
        liquidHeader.flags = 0;
        liquidHeader.liquidType = 0;
        liquidHeader.offsetX = minX;
        liquidHeader.offsetY = minY;
        liquidHeader.width   = maxX - minX + 1 + 1;
        liquidHeader.height  = maxY - minY + 1 + 1;
        liquidHeader.liquidLevel = minHeight;

        if (maxHeight == minHeight)
            liquidHeader.flags |= MAP_LIQUID_NO_HEIGHT;

        // Not need store if flat surface
        if (CONF_allow_float_to_int && (maxHeight - minHeight) < CONF_flat_liquid_delta_limit)
            liquidHeader.flags |= MAP_LIQUID_NO_HEIGHT;

        if (!fullType)
            liquidHeader.flags |= MAP_LIQUID_NO_TYPE;

        if (liquidHeader.flags & MAP_LIQUID_NO_TYPE)
            liquidHeader.liquidType = type;
        else
            map.liquidMapSize += sizeof(liquid_entry) + sizeof(liquid_flags);

        if (!(liquidHeader.flags & MAP_LIQUID_NO_HEIGHT))
            map.liquidMapSize += sizeof(float)*liquidHeader.width*liquidHeader.height;
    }

    // map hole info
    uint16 holes[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];

    if(map.liquidMapOffset)
        map.holesOffset = map.liquidMapOffset + map.liquidMapSize;
    else
        map.holesOffset = map.heightMapOffset + map.heightMapSize;

    map.holesSize = sizeof(holes);
    memset(holes, 0, map.holesSize);

    for(int i = 0; i < ADT_CELLS_PER_GRID; ++i)
    {
        for(int j = 0; j < ADT_CELLS_PER_GRID; ++j)
        {
            adt_MCNK * cell = cells->getMCNK(i,j);
            if(!cell)
                continue;
            holes[i][j] = cell->holes;
        }
    }

    // Ok all data prepared - store it
    FILE* output=fopen(output_filename, "wb");
    if(!output)
    {
        printf("ParseMap: Can't create the output file '%s'\n", output_filename);
        return false;
    }
    fwrite(&map, sizeof(map), 1, output);
    // Store area data
    fwrite(&areaHeader, sizeof(areaHeader), 1, output);
    if (!(areaHeader.flags&MAP_AREA_NO_AREA))
        fwrite(area_flags, sizeof(area_flags), 1, output);

    // Store height data
    fwrite(&heightHeader, sizeof(heightHeader), 1, output);
    if (!(heightHeader.flags & MAP_HEIGHT_NO_HEIGHT))
    {
        if (heightHeader.flags & MAP_HEIGHT_AS_INT16)
        {
            fwrite(uint16_V9, sizeof(uint16_V9), 1, output);
            fwrite(uint16_V8, sizeof(uint16_V8), 1, output);
        }
        else if (heightHeader.flags & MAP_HEIGHT_AS_INT8)
        {
            fwrite(uint8_V9, sizeof(uint8_V9), 1, output);
            fwrite(uint8_V8, sizeof(uint8_V8), 1, output);
        }
        else
        {
            fwrite(V9, sizeof(V9), 1, output);
            fwrite(V8, sizeof(V8), 1, output);
        }
    }

    // Store liquid data if need
    if (map.liquidMapOffset)
    {
        fwrite(&liquidHeader, sizeof(liquidHeader), 1, output);
        if (!(liquidHeader.flags&MAP_LIQUID_NO_TYPE))
        {
            fwrite(liquid_entry, sizeof(liquid_entry), 1, output);
            fwrite(liquid_flags, sizeof(liquid_flags), 1, output);
        }
        if (!(liquidHeader.flags&MAP_LIQUID_NO_HEIGHT))
        {
            for (int y=0; y<liquidHeader.height;y++)
                fwrite(&liquid_height[y+liquidHeader.offsetY][liquidHeader.offsetX], sizeof(float), liquidHeader.width, output);
        }
    }

    // store hole data
    fwrite(holes, map.holesSize, 1, output);

    fclose(output);

    return true;
}

bool ConvertADT(char* mpq_filename, char* output_filename, map_id mapid, int cell_y, int cell_x)
{
    ADT_file adt;

     if (!adt.loadFile(mpq_filename))
     {
         printf("Error loading %s map adt data\n", mapid.name);
         return false;
     }

     bool result1 = ParseMap(&adt, mpq_filename, output_filename, cell_y, cell_x);
     bool result2 = ParseBuildings(&adt, mpq_filename, output_filename, mapid, cell_y, cell_x);
     return result1;
}

bool ExtractWmo()
{
    bool success = true;

    std::set<std::string> result = getFileNamesWithContains("*.wmo");
    printf("Begin Extract all WMOs\n");
    for (std::set<std::string>::const_iterator itr = result.begin(); itr != result.end() && success; ++itr)
    {
        success = ExtractSingleWmo(*itr);
    }

    if (success)
        printf("\nExtract all WMOs complete (No (fatal) errors)\n");
    else
        printf("\nExtract all WMOs complete WITH ERRORS\n");

    return success;
}


void ExtractMapsFromMpq()
{
    char mpq_filename[1024];
    char output_filename[1024];
    char mpq_map_name[1024];

    printf("Extracting maps...\n");

    uint32 map_count = ReadMapDBC();

    ReadAreaTableDBC();
    ReadLiquidTypeTableDBC();

    printf("Convert map files\n");
    for(uint32 z = 0; z < map_count; ++z)
    {
        printf("Extract %s - MapId: %d (%d/%d)                  \n", map_ids[z].name,map_ids[z].id, z+1, map_count);
        // Loadup map grid data
        sprintf(mpq_map_name, "World\\Maps\\%s\\%s.wdt", map_ids[z].name, map_ids[z].name);
        WDT_file wdt;
        if (!wdt.loadFile(mpq_map_name))
        {
            printf("Error loading %s map wdt data\n", map_ids[z].name);
            continue;
        }

        // Check for Instance WMO
        if (wdt.hasMODF())
        {
            if (wdt_MODF* wdt_modf = wdt.getMODF())
            {
                std::string dirname = std::string(szWorkDirWmo) + "/" + std::string(szWorkFileWmoAndM2);
                FILE *dirfile;
                dirfile = fopen(dirname.c_str(), "ab");
                if(!dirfile)
                {
                    printf("ExtractMapsFromMpq: FATAL-ERROR: Can't open dirfile!'%s'\n", dirname.c_str());
                    exit(0);
                }
                printf("Write WMO Instance map: %u\n", map_ids[z].id);
                char* filename;
                wdt.getMWMO()->fillFileName(filename);
                printf("filename %s\n", filename);
                transformToPath(filename, strlen(filename));
                ExtractSingleWmo(std::string(filename));
                writeWMOInstance(wdt_modf->modf, GetPlainName(filename),map_ids[z].id, 65, 65, dirfile);
                fclose(dirfile);

            }
        }
        for(uint32 y = 0; y < WDT_MAP_SIZE; ++y)
        {
            for(uint32 x = 0; x < WDT_MAP_SIZE; ++x)
            {
                if (wdt.getMAIN()->adt_list[y][x].exist)
                {
                    sprintf(mpq_filename, "World\\Maps\\%s\\%s_%u_%u.adt", map_ids[z].name, map_ids[z].name, x, y);
                    sprintf(output_filename, "%s/maps/%03u%02u%02u.map", output_path, map_ids[z].id, y, x);
                    ConvertADT(mpq_filename, output_filename, map_ids[z], y, x);
                }
            }
            // draw progress bar
            printf("Processing........................%d%%\r", (100 * (y+1)) / WDT_MAP_SIZE);
        }
    }
    delete [] areas;
    delete [] map_ids;
}

void ExtractDBCFiles()
{
    printf("Extracting dbc files...\n");

    std::set<std::string> dbcfiles;

    // get DBC file list
    ArchiveSetBounds archives = GetArchivesBounds();
    for(ArchiveSet::const_iterator i = archives.first; i != archives.second;++i)
        AppendDBCFileListTo(*i, dbcfiles);

    // extract DBCs
    int count = 0;
    for (std::set<std::string>::iterator iter = dbcfiles.begin(); iter != dbcfiles.end(); ++iter)
    {
        std::string filename = szWorkDirDBC;
        filename.append("/");
        filename.append(GetPlainName(iter->c_str()));

        if (ExtractFile(iter->c_str(), filename))
            ++count;
    }
    printf("Extracted %u DBC files\n\n", count);
}

void LoadLocaleMPQFiles()
{
    for (int i = 0; i < sizeof(langs)/sizeof(langs[0]); i++)
    {
        char filename[512];

        sprintf(filename,"%s/Data/%s/locale-%s.MPQ", input_path, langs[i], langs[i]);
        if (FileExists(filename))
        {
            printf("Detected locale: %s\n", langs[i]);
            locale = i;
            HANDLE localMpqHandle;
            if (!OpenArchive(filename, &localMpqHandle))
            {
                printf("Error open archive: %s\n\n", filename);
                return;
            }

            for(int j = 0; j < 99; ++j)
            {
                char ext[3] = "";
                if(j > 1)
                    sprintf(ext, "-%i", j);

                sprintf(filename,"%s/Data/%s/patch-%s%s.MPQ", input_path, langs[i], langs[i], ext);
                if(FileExists(filename))
                {
                    HANDLE localPatchMpqHandle;
                    if (!OpenArchive(filename, &localPatchMpqHandle))
                    {
                        printf("Error open archive: %s\n\n", filename);
                    }
                }
            }
        }
    }
}

void LoadCommonMPQFiles()
{
    char filename[512];
    int count = sizeof(CONF_mpq_list)/sizeof(char*);
    for(int i = 0; i < count; ++i)
    {
        sprintf(filename, "%s/Data/%s", input_path, CONF_mpq_list[i]);
        if(FileExists(filename))
        {
            HANDLE worldMpqHandle;
            if (!OpenArchive(filename, &worldMpqHandle))
            {
                printf("Error open archive: %s\n\n", filename);
            }
        }
    }

}

int main(int argc, char * arg[])
{
    printf("Map & DBC Extractor\n");
    printf("===================\n\n");

    HandleArgs(argc, arg);

    //Open MPQs
    LoadLocaleMPQFiles();
    LoadCommonMPQFiles();
    ReadBuild();

    if(CONF_extract & EXTRACT_DBC)
    {
        CreateDir(szWorkDirDBC);
        ExtractDBCFiles();
    }

    if (CONF_extract & EXTRACT_MAP)
    {
        CreateDir(szWorkDirWmo);
        ExtractWmo();
        // Extract maps
        CreateDir(szWorkDirMap);
        ExtractMapsFromMpq();
    }

    ExtractGameobjectModels();
    //Close MPQs
    CloseArchives();

    return 0;
}
