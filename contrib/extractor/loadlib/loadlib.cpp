#define _CRT_SECURE_NO_DEPRECATE

#include "loadlib.h"
#include "../util.h"

// list of mpq files for lookup most recent file version
ArchiveSet gOpenArchives;

ArchiveSetBounds GetArchivesBounds()
{
    return ArchiveSetBounds(gOpenArchives.begin(), gOpenArchives.end());
}

bool OpenArchive(char const* mpqFileName, HANDLE* mpqHandlePtr /*= NULL*/)
{
    HANDLE mpqHandle;

    if (!SFileOpenArchive(mpqFileName, 0, MPQ_OPEN_READ_ONLY, &mpqHandle))
        return false;

    gOpenArchives.push_back(mpqHandle);

    if (mpqHandlePtr)
        *mpqHandlePtr = mpqHandle;

    return true;
}

bool OpenNewestFile(char const* filename, HANDLE* fileHandlerPtr)
{
    for(ArchiveSet::const_reverse_iterator i=gOpenArchives.rbegin(); i!=gOpenArchives.rend();++i)
    {
        // always prefer get updated file version
        if (SFileOpenFileEx(*i, filename, SFILE_OPEN_FROM_MPQ, fileHandlerPtr))
            return true;
    }

    return false;
}

bool ExtractFile( char const* mpq_name, std::string const& filename )
{
    for(ArchiveSet::const_reverse_iterator i=gOpenArchives.rbegin(); i!=gOpenArchives.rend();++i)
    {
        HANDLE fileHandle;
        if (!SFileOpenFileEx(*i, mpq_name, SFILE_OPEN_FROM_MPQ, &fileHandle))
            continue;

        if (SFileGetFileSize(fileHandle, NULL) == 0)              // some files removed in next updates and its reported  size 0
        {
            SFileCloseFile(fileHandle);
            return true;
        }

        SFileCloseFile(fileHandle);

        if (!SFileExtractFile(*i, mpq_name, filename.c_str(), SFILE_OPEN_FROM_MPQ))
        {
            printf("Can't extract file: %s\n", mpq_name);
            return false;
        }

        return true;
    }

    printf("Extracting file not found: %s\n", filename.c_str());
    return false;
}

std::set<std::string> getFileNamesWithContains(std::string contains)
{
    std::set<std::string> result;
    result.clear();
    for(ArchiveSet::const_iterator i = gOpenArchives.begin(); i != gOpenArchives.end();++i)
    {
        SFILE_FIND_DATA findData;
        if (HANDLE findHandle = SListFileFindFirstFile(*i, NULL,contains.c_str(), &findData))
        {
            result.insert(findData.cFileName);
            while (SListFileFindNextFile(findHandle,&findData))
            {
                result.insert(findData.cFileName);
            }
            SListFileFindClose(findHandle);
        }

    }
    return result;
}


void CloseArchives()
{
    for(ArchiveSet::const_iterator i = gOpenArchives.begin(); i != gOpenArchives.end();++i)
        SFileCloseArchive(*i);
    gOpenArchives.clear();
}

FileLoader::FileLoader()
{
    data = 0;
    data_size = 0;
    version = 0;
    pointer = 0;
    eof = false;
    version17 = false;
}

FileLoader::~FileLoader()
{
    free();
}

bool FileLoader::loadFile(char const*filename, bool log /*= true */)
{
    free();

    HANDLE fileHandle = 0;

    if (!OpenNewestFile(filename, &fileHandle))
    {
        if (log)
            printf("No such file %s\n", filename);
        return false;
    }

    data_size = SFileGetFileSize(fileHandle, NULL);
    if (!data_size)
    {
        if (log)
            printf("file %s has no size\n", filename);
        SFileCloseFile(fileHandle);
        return false;
    }

    data = new uint8 [data_size];
    if (!data)
    {
        if (log)
            printf("file %s has no size\n", filename);
        SFileCloseFile(fileHandle);
        return false;
    }

    if (!SFileReadFile(fileHandle, data, data_size, NULL, NULL))
    {
        if (log)
            printf("Can't read file %s\n", filename);
        SFileCloseFile(fileHandle);
        return false;
    }

    SFileCloseFile(fileHandle);
    char const* ext = GetExtension(filename);

    // wmo files have version 17
    if (strcmp(ext,".wmo") == 0 || strcmp(ext,".WMO") == 0)
        version17 = true;

    this->filename = filename;
    if (!prepareLoadedData())
    {
        printf("Error loading %s\n", filename);
        free();
        return false;
    }

    return true;
}

bool FileLoader::prepareLoadedData()
{
    // Check version
    version = (file_MVER *) data;

    if (!compareHeaderName(version->header, "MVER"))
    {
        printf("ERROR: no MVER tag");
        return false;
    }

    if (version->ver != (version17 ? FILE_FORMAT_VERSION_WMO : FILE_FORMAT_VERSION))
    {
        printf("ERROR: wrong FILE_FORMAT_VERSION %u != %u\n", version->ver, version17 ? FILE_FORMAT_VERSION_WMO : FILE_FORMAT_VERSION);
        return false;
    }
    return true;
}

size_t FileLoader::read(void* dest, size_t bytes)
{
    if (eof)
        return 0;

    size_t rpos = pointer + bytes;
    if (rpos > data_size) {
        bytes = data_size - pointer;
        eof = true;
    }

    memcpy(dest, &(data[pointer]), bytes);

    pointer = rpos;

    return bytes;
}

void FileLoader::seek(int offset)
{
    pointer = offset;
    eof = (pointer >= data_size);
}

void FileLoader::seekRelative(int offset)
{
    pointer += offset;
    eof = (pointer >= data_size);
}

void FileLoader::free()
{
    if (data) delete[] data;
    data = 0;
    data_size = 0;
    version = 0;
    pointer = 0;
    eof = false;
}
