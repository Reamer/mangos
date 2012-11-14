#include <stddef.h>
#include "util.h"

char const* GetPlainName(char const* fileName)
{
    const char * szTemp;

    if((szTemp = strrchr(fileName, '\\')) != NULL)
        fileName = szTemp + 1;
    return fileName;
}

char * GetPlainName(char * fileName)
{
    char * szTemp;

    if((szTemp = strrchr(fileName, '\\')) != NULL)
        fileName = szTemp + 1;
    return fileName;
}

char const* GetExtension(char const* fileName)
{
    char const* szTemp;
    if((szTemp = strrchr(fileName, '.')) != NULL)
        return szTemp;
    return NULL;
}

void CreateDir( const std::string& path )
{
    #ifdef WIN32
    _mkdir( path.c_str());
    #else
    mkdir( path.c_str(), 0777 );
    #endif
}

bool FileExists(char const* fileName )
{
    int fp = _open(fileName, OPEN_FLAGS);
    if(fp != -1)
    {
        _close(fp);
        return true;
    }

    return false;
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
void changeWhitespaceToUnderscore(std::string &name)
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
void transformToPath(std::string &name)
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

bool compareHeaderName(ChunkHeader header, std::string headername)
{
    if (headername.size() != 4)
    {
        printf("ERROR: Headername Size is not 4");
        return false;
    }
    if (header.fcc_txt[3] == headername.at(0) &&
        header.fcc_txt[2] == headername.at(1) &&
        header.fcc_txt[1] == headername.at(2) &&
        header.fcc_txt[0] == headername.at(3))
        return true;
    return false;
}
void changeMDXtoM2(std::string &name)
{
    char const* ext = GetExtension(name.c_str());

    // < 3.1.0 ADT MMDX section store filename.mdx filenames for corresponded .m2 file
    if (!strcmp(ext, ".mdx"))
    {
        // replace .mdx -> .m2
        name.erase(name.length()-2,2);
        name.append("2");
    }
    // >= 3.1.0 ADT MMDX section store filename.m2 filenames for corresponded .m2 file
    // nothing do
}
