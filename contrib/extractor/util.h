#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>
#include "loadlib/loadlib.h"
#include "vec3d.h"

#if defined( __GNUC__ )
    #define _open   open
    #define _close close
    #ifndef O_BINARY
        #define O_BINARY 0
    #endif
#else
    #include <io.h>
#endif

#ifdef O_LARGEFILE
    #define OPEN_FLAGS  (O_RDONLY | O_BINARY | O_LARGEFILE)
#else
    #define OPEN_FLAGS (O_RDONLY | O_BINARY)
#endif

char const* GetPlainName(char const* fileName);
char * GetPlainName(char * FileName);
char const* GetExtension(char const* fileName);
void CreateDir( const std::string& Path );
bool FileExists(char const* fileName );
std::vector<char*> splitFileNamesAtDelim(char* filenames, uint32 size, char delim);
void changeWhitespaceToUnderscore(char* name, size_t len);
void changeWhitespaceToUnderscore(std::string &name);
void transformToPath(char* name, size_t len);
void transformToPath(std::string &name);
bool compareHeaderName(ChunkHeader header, std::string headername);
void changeMDXtoM2(std::string &name);
/* for whatever reason a certain company just can't stick to one coordinate system... */
static inline Vec3D fixCoordSystem(const Vec3D &v) { return Vec3D(v.x, v.z, -v.y);}
static inline Vec3D fixCoordSystem2(const Vec3D &v) { return Vec3D(v.x, v.z, v.y);}
static inline Vec3D fixCoords(const Vec3D &v){ return Vec3D(v.z, v.x, v.y);}

#endif
