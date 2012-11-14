#ifndef LOAD_LIB_H
#define LOAD_LIB_H

#ifdef _DLL
#undef _DLL
#endif

#include <string>
#include <set>
#include "StormLib.h"
#include <deque>

#ifdef WIN32
typedef __int64            int64;
typedef __int32            int32;
typedef __int16            int16;
typedef __int8             int8;
typedef unsigned __int64   uint64;
typedef unsigned __int32   uint32;
typedef unsigned __int16   uint16;
typedef unsigned __int8    uint8;
#else
#include <stdint.h>
#ifndef uint64_t
#ifdef __linux__
#include <linux/types.h>
#endif
#endif
typedef int64_t            int64;
typedef int32_t            int32;
typedef int16_t            int16;
typedef int8_t             int8;
typedef uint64_t           uint64;
typedef uint32_t           uint32;
typedef uint16_t           uint16;
typedef uint8_t            uint8;
#endif

typedef std::deque<HANDLE> ArchiveSet;
typedef std::pair<ArchiveSet::const_iterator,ArchiveSet::const_iterator> ArchiveSetBounds;

bool OpenArchive(char const* mpqFileName, HANDLE* mpqHandlePtr = NULL);
bool OpenNewestFile(char const* filename, HANDLE* fileHandlerPtr);
ArchiveSetBounds GetArchivesBounds();
bool ExtractFile( char const* mpq_name, std::string const& filename );
void CloseArchives();
std::set<std::string> getFileNamesWithContains(std::string contains);

#define FILE_FORMAT_VERSION    18
#define FILE_FORMAT_VERSION_WMO 17


struct ChunkHeader
{
    union{
        uint32 fcc;
        char   fcc_txt[4];
    };
    uint32 size;
    ChunkHeader() : fcc(0), size(0) {}
};
//
// File version chunk
//
struct file_MVER
{
    ChunkHeader header;
    uint32 ver;
    file_MVER() : ver(0), header() {}
};

class FileLoader{
    uint8  *data;
    uint32  data_size;
    bool eof;
    uint64 pointer;
    bool version17;
    std::string filename;
public:
    virtual bool prepareLoadedData();
    uint8* getData()     {return data;}
    uint32 getDataSize() {return data_size;}
    std::string getFilename() { return filename;}
    size_t read(void* dest, size_t bytes);
    size_t getPos() { return pointer; }
    uint8* getPointer() { return data + pointer; }
    bool isEof() { return eof; }
    void seek(int offset);
    void seekRelative(int offset);

    file_MVER *version;
    FileLoader();
    virtual ~FileLoader();
    bool loadFile(char const*filename, bool log = true);
    virtual void free();
};
#endif
