#define _CRT_SECURE_NO_DEPRECATE

#include "adt.h"

// Helper
int holetab_h[4] = {0x1111, 0x2222, 0x4444, 0x8888};
int holetab_v[4] = {0x000F, 0x00F0, 0x0F00, 0xF000};

bool isHole(int holes, int i, int j)
{
    int testi = i / 2;
    int testj = j / 4;
    if(testi > 3) testi = 3;
    if(testj > 3) testj = 3;
    return (holes & holetab_h[testi] & holetab_v[testj]) != 0;
}
std::vector<char*> splitFileNamesAtDelim(char* filenames, uint32 size, char delim)
{
    std::vector<char*> result;
    result.push_back(filenames);
    for (uint32 i = 0;i<size; ++i)
    {
        if (filenames[i]==delim && i + 1 < size)
            result.push_back(&filenames[i+1]);
    }
    return result;
}
// oldname fixname2
void changeWhitespaceToUnderscore(char* name, size_t len)
{
    for (size_t i=0; i<len-3; i++)
    {
        if(name[i] == ' ')
        name[i] = '_';
    }
}

// oldname fixname2
void changeWhitespaceToUnderscore(std::string name)
{
    size_t found = name.find(" ");
    while (found != std::string::npos)
    {
        name.replace(found, 1, "_");
        found = name.find(" ");
    }
}

//oldname fixnamen
void transformToPath(char* name, size_t len)
{
    for (size_t i=0; i<len-3; i++)
    {
        if (i>0 && name[i]>='A' && name[i]<='Z' && isalpha(name[i-1]))
        {
            // to lowercase
            name[i] |= 0x20;
        } else if ((i==0 || !isalpha(name[i-1])) && name[i]>='a' && name[i]<='z')
        {
            // to uppercase
            name[i] &= ~0x20;
        }
    }
    //extension in lowercase
    for(size_t i=len-3; i<len; i++)
        name[i] |= 0x20;
}

//oldname fixnamen
void transformToPath(std::string name)
{
    for (size_t i=0; i<name.size()-3; i++)
    {
        if (i>0 && name.at(i)>='A' && name.at(i)<='Z' && isalpha(name.at(i-1)))
        {
            // to lowercase
            char newchar = name.at(i);
            newchar = tolower(newchar);
            name.replace(i,1,1, newchar);
        }
        else if ((i==0 || !isalpha(name.at(i-1))) && name.at(i)>='a' && name.at(i)<='z')
        {
            // to uppercase
            char newchar = name.at(i);
            newchar = toupper(newchar);
            name.replace(i,1,1, newchar);
        }
    }
    //extension in lowercase
    for(size_t i = name.size() - 3; i<name.size(); ++i)
    {
        char newchar = name.at(i);
        newchar = tolower(newchar);
        name.replace(i,1,1, newchar);
    }
}

//
// Adt file loader class
//
ADT_file::ADT_file()
{
    a_grid = 0;
}

ADT_file::~ADT_file()
{
    free();
}

void ADT_file::free()
{
    a_grid = 0;
    FileLoader::free();
}

//
// Adt file check function
//
bool ADT_file::prepareLoadedData()
{
    // Check parent
    if (!FileLoader::prepareLoadedData())
    {
        printf("ERROR: FileLoaderCheck failed\n");
        return false;
    }

    // Check and prepare MHDR
    a_grid = (adt_MHDR *)(GetData()+sizeof(file_MVER));
    if (!a_grid->prepareLoadedData())
    {
        printf("ERROR: HeaderCheck failed\n");
        return false;
    }

    return true;
}

bool adt_MHDR::prepareLoadedData()
{
    if (header.fcc != 'MHDR')
        return false;

    if (header.size!=sizeof(adt_MHDR) - sizeof(chunkHeader))
    {
        printf("ERROR: SizeCheck failed\n");
        return false;
    }

    // Check and prepare MCIN
    if (offsMCIN && !getMCIN()->prepareLoadedData())
    {
        printf("ERROR: MCIN-Check failed\n");
        return false;
    }

    // Check and prepare MH2O
    if (offsMH2O && !getMH2O()->prepareLoadedData())
    {
        printf("ERROR: MH2O-Check failed\n");
        return false;
    }

    /* check for M2 or mdx (doodads) */
    adt_MMDX* mdx = getMMDX();
    adt_MMID* mid = getMMID();
    adt_MDDF* ddf = getMDDF();
    // single checks
    if (mdx && !mdx->prepareLoadedData())
    {
        printf("ERROR: MMDX-Check failed\n");
        return false;
    }

    if (mid && !mid->prepareLoadedData())
    {
        printf("ERROR: MMID-Check failed\n");
        return false;
    }

    if (ddf && !ddf->prepareLoadedData())
    {
        printf("ERROR: MDDF-Check failed\n");
        return false;
    }

    // mix check
    if (mdx && mid && ddf)
    {
        uint32 mdxSize = mdx->getFileNames().size();
        uint32 midSize = mid->getMaxM2Models();
        if (midSize != mdxSize)
        {
            printf("ERROR: Entry amount is wrong, should be equal: MMDX (%u), MMID (%u)\n", mdxSize, midSize);
            return false;
        }
    }

    /* check for WMOs (MapObjects) */
    adt_MWMO* wmo = getMWMO();
    adt_MWID* wid = getMWID();
    adt_MODF* odf = getMODF();
    // single checks
    if (wmo && !wmo->prepareLoadedData())
    {
        printf("ERROR: MWMO-Check failed\n");
        return false;
    }
    if (wid && !wid->prepareLoadedData())
    {
        printf("ERROR: MWID-Check failed\n");
        return false;
    }
    if (odf && !odf->prepareLoadedData())
    {
        printf("ERROR: MODF-Check failed\n");
        return false;
    }
    // mix check
    if (wmo && wid && odf)
    {
        uint32 wmoSize = wmo->getFileNames().size();
        uint32 widSize = wid->getMaxWMO();
        if (wmoSize != widSize)
        {
            printf("ERROR: Entry amount is wrong, should be equal: MWMO (%u), MWID (%u)\n", wmoSize, widSize);
            return false;
        }
    }
    return true;
}

bool adt_MCIN::prepareLoadedData()
{
    if (header.fcc != 'MCIN')
        return false;

    // Check cells data
    for (int i=0; i<ADT_CELLS_PER_GRID;i++)
        for (int j=0; j<ADT_CELLS_PER_GRID;j++)
            if (cells[i][j].offsMCNK && !getMCNK(i,j)->prepareLoadedData())
                return false;

    return true;
}

bool adt_MH2O::prepareLoadedData()
{
    if (header.fcc != 'MH2O')
        return false;

    // Check liquid data
//    for (int i=0; i<ADT_CELLS_PER_GRID;i++)
//        for (int j=0; j<ADT_CELLS_PER_GRID;j++)

    return true;
}

bool adt_MCNK::prepareLoadedData()
{
    if (header.fcc != 'MCNK')
        return false;

    // Check height map
    if (offsMCVT && !getMCVT()->prepareLoadedData())
        return false;
    // Check liquid data
    if (offsMCLQ && !getMCLQ()->prepareLoadedData())
        return false;

    return true;
}

bool adt_MCVT::prepareLoadedData()
{
    if (header.fcc != 'MCVT')
        return false;

    if (header.size != sizeof(adt_MCVT) - sizeof(chunkHeader))
        return false;

    return true;
}

bool adt_MCLQ::prepareLoadedData()
{
    if (header.fcc != 'MCLQ')
        return false;

    return true;
}

bool adt_MMID::prepareLoadedData()
{
    if (header.fcc != 'MMID')
        return false;

    if (header.size && (header.size % sizeof(uint32) != 0))
    {
        printf("ERROR: size %u is no divisor from %zu", header.size, sizeof(uint32));
        return false;
    }

    return true;
}

std::vector<uint32> adt_MMID::getOffsetList()
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

FilenameInfo adt_MMID::getMMDXInfo(uint32 value)
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

bool adt_MMDX::prepareLoadedData()
{
    if (header.fcc != 'MMDX')
        return false;

    return true;
}

std::vector<char*> adt_MMDX::getFileNames()
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

void adt_MMDX::getM2Model(FilenameInfo* info)
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

bool adt_MWMO::prepareLoadedData()
{
    if (header.fcc != 'MWMO')
        return false;

    return true;
}

std::vector<char*> adt_MWMO::getFileNames()
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

void adt_MWMO::getWMOFilename(FilenameInfo* info)
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

bool adt_MWID::prepareLoadedData()
{
    if (header.fcc != 'MWID')
        return false;

    if (header.size && (header.size % sizeof(uint32) != 0))
    {
        printf("ERROR: size %u is no divisor from %zu", header.size, sizeof(uint32));
        return false;
    }

    return true;
}

std::vector<uint32> adt_MWID::getOffsetList()
{
    std::vector<uint32> result;
    for (uint32 i = 0; i < getMaxWMO(); ++i)
    {
        uint32 j;
        memcpy(&j, ((uint8*)this + 8 + 4*i), sizeof(uint32));
        result.push_back(j);
    }
    return result;
}

FilenameInfo adt_MWID::getMWMOInfo(uint32 value)
{
    // create Info
    FilenameInfo info;
    if (value > getMaxWMO())
    {
        printf("Value: %u , MaxValue: %u\n", value, getMaxWMO());
        return info;
    }
    if (header.size)
    {
        memcpy(&info.offset, ((uint8*)this + sizeof(chunkHeader) + sizeof(uint32)*value), sizeof(info.offset));
        value++;
        if (value < getMaxWMO())
        {
            uint32 nextoffset;
            memcpy(&nextoffset, ((uint8*)this + sizeof(chunkHeader) + sizeof(uint32)*value), sizeof(uint32));
            info.length = nextoffset - info.offset;
        }
    }
    return info;
}

bool adt_MDDF::prepareLoadedData()
{
    if (header.fcc != 'MDDF')
        return false;

    if (header.size && header.size % sizeof(MDDF_Entry) != 0)
    {
        printf("ERROR: MDDF size check failed");
        return false;
    }

    return true;
}

MDDF_Entry adt_MDDF::getMDDF_Entry(uint32 value)
{
    MDDF_Entry entry;
    if (value < getMaxEntries())
        memcpy(&entry, ((uint8*)this + sizeof(chunkHeader) + sizeof(MDDF_Entry)*value), sizeof(MDDF_Entry));
    return entry;
}

bool adt_MODF::prepareLoadedData()
{
    if (header.fcc != 'MODF')
        return false;

    if (header.size && header.size % sizeof(MODF_Entry) != 0)
    {
        printf("ERROR: MDDF size check failed");
        return false;
    }

    return true;
}

MODF_Entry adt_MODF::getMODF_Entry(uint32 value)
{
    MODF_Entry entry;
    if (value < getMaxEntries())
        memcpy(&entry, ((uint8*)this + sizeof(chunkHeader) + sizeof(MODF_Entry)*value), sizeof(MODF_Entry));
    return entry;

}

void MODF_Entry::printMODF_Entry(MODF_Entry* entry)
{
    printf("ID: %u\n"
            "uniqueId %u\n"
            "position %f ; %f ; %f\n"
            "rotation %f ; %f ; %f\n"
            "flags %u ; doodadSet %u ;"
            "nameSet %u ; padding %u"
            , entry->mwidEntry, entry->uniqueId, entry->position[0], entry->position[1], entry->position[2] , entry->rotation[0], entry->rotation[1], entry->rotation[2],
            entry->flags, entry->doodadSet, entry->nameSet, entry->padding);
}

void MDDF_Entry::printMDDF_Entry(MDDF_Entry* entry)
{
    printf("ID: %u\n"
            "uniqueId %u\n"
            "position %f ; %f ; %f\n"
            "rotation %f ; %f ; %f\n"
            "scale %u ; flags %u"
            , entry->mmidEntry, entry->uniqueId, entry->position[0], entry->position[1], entry->position[2] , entry->rotation[0], entry->rotation[1], entry->rotation[2],
            entry->scale, entry->flags);
}
