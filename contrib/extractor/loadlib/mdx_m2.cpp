#include "mdx_m2.h"
#include "../extractor.h"


MDX_M2_file::MDX_M2_file() : vertices(0), indices(0)
{
}

MDX_M2_file::~MDX_M2_file()
{
    _unload();
    free();
}

void MDX_M2_file::free()
{
    FileLoader::free();
}

bool MDX_M2_file::prepareLoadedData()
{
    memcpy(&header, getData(), sizeof(ModelHeader));
    if(header.nBoundingTriangles > 0)
    {
        seek(0);
        seekRelative(header.ofsBoundingVertices);
        vertices = new Vec3D[header.nBoundingVertices];
        read(vertices,header.nBoundingVertices*12);
        for (uint32 i=0; i<header.nBoundingVertices; i++)
        {
            vertices[i] = fixCoordSystem(vertices[i]);
        }
        seek(0);
        seekRelative(header.ofsBoundingTriangles);
        indices = new uint16[header.nBoundingTriangles];
        read(indices,header.nBoundingTriangles*2);
    }
    return true;
}

bool MDX_M2_file::ConvertToVMAPModel(const char * outfilename)
{
    if (header.nBoundingTriangles == 0)
    {
        // Thats a SPAMER because a ot of objects has no bounding Triangles, like plants
        //printf("\"%s\" not included, because no bounding Triangles\n", outfilename);
        return false;
    }
    int N[12] = {0,0,0,0,0,0,0,0,0,0,0,0};
    FILE * output=fopen(outfilename,"wb");
    if(!output)
    {
        printf("Can't create the output file '%s'\n",outfilename);
        return false;
    }
    fwrite(RAW_VMAP_MAGIC,8,1,output);
    uint32 nVertices = 0;
    nVertices = header.nBoundingVertices;
    fwrite(&nVertices, sizeof(int), 1, output);
    uint32 nofgroups = 1;
    fwrite(&nofgroups,sizeof(uint32), 1, output);
    fwrite(N,4*3,1,output);// rootwmoid, flags, groupid
    fwrite(N,sizeof(float),3*2,output);//bbox, only needed for WMO currently
    fwrite(N,4,1,output);// liquidflags
    fwrite("GRP ",4,1,output);
    uint32 branches = 1;
    int wsize;
    wsize = sizeof(branches) + sizeof(uint32) * branches;
    fwrite(&wsize, sizeof(int), 1, output);
    fwrite(&branches,sizeof(branches), 1, output);
    uint32 nIndexes = 0;
    nIndexes = header.nBoundingTriangles;
    fwrite(&nIndexes,sizeof(uint32), 1, output);
    fwrite("INDX",4, 1, output);
    wsize = sizeof(uint32) + sizeof(unsigned short) * nIndexes;
    fwrite(&wsize, sizeof(int), 1, output);
    fwrite(&nIndexes, sizeof(uint32), 1, output);
    if(nIndexes >0)
    {
        fwrite(indices, sizeof(unsigned short), nIndexes, output);
    }
    fwrite("VERT",4, 1, output);
    wsize = sizeof(int) + sizeof(float) * 3 * nVertices;
    fwrite(&wsize, sizeof(int), 1, output);
    fwrite(&nVertices, sizeof(int), 1, output);
    if(nVertices >0)
    {
        for(uint32 vpos=0; vpos <nVertices; ++vpos)
        {
            std::swap(vertices[vpos].y, vertices[vpos].z);
        }
        fwrite(vertices, sizeof(float)*3, nVertices, output);
    }

    fclose(output);

    return true;
}
