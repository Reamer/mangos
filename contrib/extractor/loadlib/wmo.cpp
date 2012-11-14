#include "wmo.h"
#include "../extractor.h"

bool WMORoot::prepareLoadedData()
{
    // Check parent
    if (!FileLoader::prepareLoadedData())
    {
        printf("ERROR: FileLoaderCheck failed\n");
        return false;
    }

    memcpy(&m_wohd, getData()+sizeof(file_MVER), sizeof(wmo_MOHD));

    if (!getMOHD())
    {
        printf("ERROR: No WOHD");
        return false;
    }
    // Check and prepare MHDR
    if (!getMOHD()->prepareLoadedData())
    {
        printf("ERROR: MHDR-Check failed\n");
        return false;
    }
    return true;
}

bool WMORoot::ConvertToVMAPRootWmo(FILE *pOutfile)
{
    //printf("Convert RootWmo...\n");

    fwrite(RAW_VMAP_MAGIC,1,8,pOutfile);
    unsigned int nVectors = 0;
    fwrite(&nVectors,sizeof(nVectors),1,pOutfile); // will be filled later
    fwrite(&getMOHD()->nGroups,4,1,pOutfile);
    fwrite(&getMOHD()->RootWMOID,4,1,pOutfile);
    return true;
}

bool WMOGroup::prepareLoadedData()
{
    // Check parent
    if (!FileLoader::prepareLoadedData())
    {
        printf("ERROR: FileLoaderCheck failed\n");
        return false;
    }
    ChunkHeader header;
    while (!isEof())
    {
        read(&header, sizeof(ChunkHeader));
        if (compareHeaderName(header, "MOGP"))
        {
            header.size = 68;
        }
        size_t nextpos = getPos() + header.size;

        if (compareHeaderName(header, "MOGP"))//header
        {
            m_MOGP.header = header;
            read(&m_MOGP.groupName, 4);
            read(&m_MOGP.descGroupName, 4);
            read(&m_MOGP.mogpFlags, 4);
            read(&m_MOGP.bbcorn1, 12);
            read(&m_MOGP.bbcorn2, 12);
            read(&m_MOGP.moprIdx, 2);
            read(&m_MOGP.moprNItems, 2);
            read(&m_MOGP.nBatchA, 2);
            read(&m_MOGP.nBatchB, 2);
            read(&m_MOGP.nBatchC, 4);
            read(&m_MOGP.fogIdx, 4);
            read(&m_MOGP.liquidType, 4);
            read(&m_MOGP.groupWMOID,4);
            if (!getMOGP()->prepareLoadedData())
            {
                printf("ERROR: MOGP-Check failed\n");
                return false;
            }
        }
        else if (compareHeaderName(header, "MOPY"))
        {
            MOPY = new char[header.size];
            mopy_size = header.size;
            nTriangles = header.size / 2;
            read(MOPY, header.size);
            /*
            m_MOPY = getPointer() - sizeof(ChunkHeader);
            if (!getMOPY()->prepareLoadedData())
            {
                printf("ERROR: MOPY-Check failed\n");
                return false;
            }*/
        }
        else if (compareHeaderName(header, "MOVI"))
        {
            MOVI = new uint16[header.size/2];
            read(MOVI, header.size);
            /*
            m_MOVI = getPointer() - sizeof(ChunkHeader);
            if (!getMOVI()->prepareLoadedData())
            {
                printf("ERROR: MOVI-Check failed\n");
                return false;
            }*/
        }
        else if (compareHeaderName(header, "MOVT"))
        {
            MOVT = new float[header.size/4];
            read(MOVT, header.size);
            nVertices = header.size / 12;
            /*m_MOVT = getPointer() - sizeof(ChunkHeader);
            if (!getMOVT()->prepareLoadedData())
            {
                printf("ERROR: MOVT-Check failed\n");
                return false;
            }*/
        }
        else if (compareHeaderName(header, "MONR"))
        {
            m_MONR = getPointer() - sizeof(ChunkHeader);
        }
        else if (compareHeaderName(header, "MOTV"))
        {
            m_MOTV = getPointer() - sizeof(ChunkHeader);
        }
        else if (compareHeaderName(header, "MOBA"))
        {
            MOBA = new uint16[header.size/2];
            moba_size = header.size/2;
            read(MOBA, header.size);
            /*
            m_MOBA = getPointer() - sizeof(ChunkHeader);
            if (!getMOBA()->prepareLoadedData())
            {
                printf("ERROR: MOBA-Check failed\n");
                return false;
            }*/
        }
        else if (compareHeaderName(header, "MLIQ"))
        {
            liquflags |= 1;

            m_MLIQ.header = header;
            read(&m_MLIQ.liquidHeader, sizeof(WMOLiquidHeader));
            LiquEx = new WMOLiquidVert[getMLIQ()->getLiqVertMax()];
            read(LiquEx, getMLIQ()->getLiqVertSize());
            LiquBytes = new char[getMLIQ()->getLiquidBytesMax()];
            read(LiquBytes, getMLIQ()->getLiquidBytesSize());

            if (!getMLIQ()->prepareLoadedData())
            {
                printf("ERROR: MLIQ-Check failed\n");
                return false;
            }
        }
        else
        {
            //printf("unknown chunk %s\n", header.fcc_txt);
        }
        seek(nextpos);
    }
    return true;
}

int WMOGroup::ConvertToVMAPGroupWmo(FILE *output, WMORoot *rootWMO, bool pPreciseVectorData)
{
    fwrite(&getMOGP()->mogpFlags,sizeof(uint32),1,output);
    fwrite(&getMOGP()->groupWMOID,sizeof(uint32),1,output);
    // group bound
    fwrite(getMOGP()->bbcorn1, sizeof(float), 3, output);
    fwrite(getMOGP()->bbcorn2, sizeof(float), 3, output);
    fwrite(&liquflags,sizeof(uint32),1,output);
    uint32 nColTriangles = 0;
    if(pPreciseVectorData)
    {
        char GRP[] = "GRP ";
        fwrite(GRP,1,4,output);

        int k = 0;
        int moba_batch = moba_size/12;
        MobaEx = new int[moba_batch*4];
        for(int i=8; i<moba_size; i+=12)
        {
            MobaEx[k++] = MOBA[i];
        }
        int moba_size_grp = moba_batch*4+4;
        fwrite(&moba_size_grp,4,1,output);
        fwrite(&moba_batch,4,1,output);
        fwrite(MobaEx,4,k,output);
        delete [] MobaEx;

        uint32 nIdexes = nTriangles * 3;

        if(fwrite("INDX",4, 1, output) != 1)
        {
            printf("Error while writing file nbraches ID");
            exit(0);
        }
        int wsize = sizeof(uint32) + sizeof(unsigned short) * nIdexes;
        if(fwrite(&wsize, sizeof(int), 1, output) != 1)
        {
            printf("Error while writing file wsize");
            // no need to exit?
        }
        if(fwrite(&nIdexes, sizeof(uint32), 1, output) != 1)
        {
            printf("Error while writing file nIndexes");
            exit(0);
        }
        if(nIdexes >0)
        {
            if(fwrite(MOVI, sizeof(unsigned short), nIdexes, output) != nIdexes)
            {
                printf("Error while writing file indexarray");
                exit(0);
            }
        }

        if(fwrite("VERT",4, 1, output) != 1)
        {
            printf("Error while writing file nbraches ID");
            exit(0);
        }
        wsize = sizeof(int) + sizeof(float) * 3 * nVertices;
        if(fwrite(&wsize, sizeof(int), 1, output) != 1)
        {
            printf("Error while writing file wsize");
            // no need to exit?
        }
        if(fwrite(&nVertices, sizeof(int), 1, output) != 1)
        {
            printf("Error while writing file nVertices");
            exit(0);
        }
        if(nVertices >0)
        {
            if(fwrite(MOVT, sizeof(float)*3, nVertices, output) != nVertices)
            {
                printf("Error while writing file vectors");
                exit(0);
            }
        }

        nColTriangles = nTriangles;
    }
    else
    {
        char GRP[] = "GRP ";
        fwrite(GRP,1,4,output);
        int k = 0;
        int moba_batch = moba_size/12;
        MobaEx = new int[moba_batch*4];
        for(int i=8; i<moba_size; i+=12)
        {
            MobaEx[k++] = MOBA[i];
        }

        int moba_size_grp = moba_batch*4+4;
        fwrite(&moba_size_grp,4,1,output);
        fwrite(&moba_batch,4,1,output);
        fwrite(MobaEx,4,k,output);
        delete [] MobaEx;

        //-------INDX------------------------------------
        //-------MOPY--------
        MoviEx = new uint16[nTriangles*3]; // "worst case" size...
        int *IndexRenum = new int[nVertices];
        memset(IndexRenum, 0xFF, nVertices*sizeof(int));
        for (int i=0; i<nTriangles; ++i)
        {
            // Skip no collision triangles
            if (MOPY[2*i]&WMO_MATERIAL_NO_COLLISION ||
              !(MOPY[2*i]&(WMO_MATERIAL_HINT|WMO_MATERIAL_COLLIDE_HIT)) )
                continue;
            // Use this triangle
            for (int j=0; j<3; ++j)
            {
                IndexRenum[MOVI[3*i + j]] = 1;
                MoviEx[3*nColTriangles + j] = MOVI[3*i + j];
            }
            ++nColTriangles;
        }

        // assign new vertex index numbers
        int nColVertices = 0;
        for (uint32 i=0; i<nVertices; ++i)
        {
            if (IndexRenum[i] == 1)
            {
                IndexRenum[i] = nColVertices;
                ++nColVertices;
            }
        }

        // translate triangle indices to new numbers
        for (int i=0; i<3*nColTriangles; ++i)
        {
            assert(MoviEx[i] < nVertices);
            MoviEx[i] = IndexRenum[MoviEx[i]];
        }

        // write triangle indices
        int INDX[] = {0x58444E49, nColTriangles*6+4, nColTriangles*3};
        fwrite(INDX,4,3,output);
        fwrite(MoviEx,2,nColTriangles*3,output);

        // write vertices
        int VERT[] = {0x54524556, nColVertices*3*sizeof(float)+4, nColVertices};// "VERT"
        int check = 3*nColVertices;
        fwrite(VERT,4,3,output);
        for (uint32 i=0; i<nVertices; ++i)
            if(IndexRenum[i] >= 0)
                check -= fwrite(MOVT+3*i, sizeof(float), 3, output);

        assert(check==0);

        delete [] MoviEx;
        delete [] IndexRenum;
    }

    //------LIQU------------------------
    if(getMLIQ()->getLiqVertSize())
    {
        uint32 LIQU_h[] = {0x5551494C, sizeof(WMOLiquidHeader) + getMLIQ()->getLiqVertSize() + getMLIQ()->getLiquidBytesSize()};// "LIQU"
        fwrite(LIQU_h, 4, 2, output);

        // according to WoW.Dev Wiki:
        uint32 liquidEntry;
        if (rootWMO->getMOHD()->liquidType & 4)
            liquidEntry = getMOGP()->liquidType;
        else if (getMOGP()->liquidType == 15)
            liquidEntry = 0;
        else
            liquidEntry = getMOGP()->liquidType + 1;

        if (!liquidEntry)
        {
            int v1; // edx@1
            int v2; // eax@1

            v1 = getMLIQ()->getLiquidBytesMax();
            v2 = 0;
            if (v1 > 0)
            {
                while ((LiquBytes[v2] & 0xF) == 15)
                {
                    ++v2;
                    if (v2 >= v1)
                        break;
                }

                if (v2 < v1 && (LiquBytes[v2] & 0xF) != 15)
                    liquidEntry = (LiquBytes[v2] & 0xF) + 1;
            }
        }

        if (liquidEntry && liquidEntry < 21)
        {
            switch (((uint8)liquidEntry - 1) & 3)
            {
                case 0:
                    liquidEntry = ((getMOGP()->mogpFlags & 0x80000) != 0) + 13;
                    break;
                case 1:
                    liquidEntry = 14;
                    break;
                case 2:
                    liquidEntry = 19;
                    break;
                case 3:
                    liquidEntry = 20;
                    break;
                default:
                    break;
            }
        }

        getMLIQ()->liquidHeader.type = liquidEntry;


        fwrite(&getMLIQ()->liquidHeader, sizeof(WMOLiquidHeader), 1, output);
        // only need height values, the other values are unknown anyway
        for (uint32 i = 0; i < getMLIQ()->getLiqVertMax(); ++i)
            fwrite(&LiquEx[i].height, sizeof(float), 1, output);
        // todo: compress to bit field
        fwrite(LiquBytes, 1, getMLIQ()->getLiquidBytesMax(), output);
    }

    return nColTriangles;
}

bool wmo_MOHD::prepareLoadedData()
{
    if (!compareHeaderName(header, "MOHD"))
        return false;
    return true;
}

bool wmo_MOGP::prepareLoadedData()
{
    if (!compareHeaderName(header, "MOGP"))
        return false;
    return true;
}

bool wmo_MOPY::prepareLoadedData()
{
    if (!compareHeaderName(header, "MOPY"))
        return false;

    if (header.size && header.size % sizeof(wmo_MOPY_Entry) != 0)
    {
        printf("ERROR: MOPY size %u is no divisor from %zu", header.size, sizeof(wmo_MOPY_Entry));
        return false;
    }
    return true;
}

wmo_MOPY_Entry wmo_MOPY::getMOPY_Entry(uint32 value)
{
    wmo_MOPY_Entry entry;
    if (value < getMaxEntries())
        memcpy(&entry, ((uint8*)this + sizeof(ChunkHeader) + value*sizeof(wmo_MOPY_Entry)), sizeof(wmo_MOPY_Entry));
    return entry;
}

bool wmo_MOVI::prepareLoadedData()
{
    if (!compareHeaderName(header, "MOVI"))
        return false;

    if (header.size && header.size % sizeof(wmo_MOVI_Entry) != 0)
    {
        printf("ERROR: MOPY size %u is no divisor from %zu", header.size, sizeof(wmo_MOVI_Entry));
        return false;
    }
    return true;
}

wmo_MOVI_Entry wmo_MOVI::getMOVI_Entry(uint32 value)
{
    wmo_MOVI_Entry entry;
    if (value < getMaxEntries())
        memcpy(&entry, ((uint8*)this + sizeof(ChunkHeader) + value*sizeof(wmo_MOVI_Entry)), sizeof(wmo_MOVI_Entry));
    return entry;
}

bool wmo_MOVT::prepareLoadedData()
{
    if (!compareHeaderName(header, "MOVT"))
        return false;

    if (header.size && header.size % sizeof(wmo_MOVT_Entry) != 0)
    {
        printf("ERROR: MOVT size %u is no divisor from %zu", header.size, sizeof(wmo_MOVT_Entry));
        return false;
    }
    return true;
}

wmo_MOVT_Entry wmo_MOVT::getMOVT_Entry(uint32 value)
{
    wmo_MOVT_Entry entry;
    if (value < getMaxEntries())
        memcpy(&entry, ((uint8*)this + sizeof(ChunkHeader) + value*sizeof(wmo_MOVT_Entry)), sizeof(wmo_MOVT_Entry));
    return entry;
}

bool wmo_MOBA::prepareLoadedData()
{
    if (!compareHeaderName(header, "MOBA"))
        return false;

    if (header.size && header.size % sizeof(wmo_MOBA_Entry) != 0)
    {
        printf("ERROR: MOVT size %u is no divisor from %zu", header.size, sizeof(wmo_MOBA_Entry));
        return false;
    }
    return true;
}

wmo_MOBA_Entry wmo_MOBA::getMOBA_Entry(uint32 value)
{
    wmo_MOBA_Entry entry;
    if (value < getMaxEntries())
        memcpy(&entry, ((uint8*)this + sizeof(ChunkHeader) + value*sizeof(wmo_MOBA_Entry)), sizeof(wmo_MOBA_Entry));
    return entry;
}

bool wmo_MLIQ::prepareLoadedData()
{
    if (!compareHeaderName(header, "MLIQ"))
        return false;
    return true;
}

WMOLiquidVert wmo_MLIQ::getLiquidVert(uint32 value)
{
    WMOLiquidVert entry;
    if (value < getLiqVertMax())
        memcpy(&entry, ((uint8*)this + sizeof(WMOLiquidHeader) + value*sizeof(WMOLiquidVert)), sizeof(WMOLiquidVert));
    return entry;
}

uint8 wmo_MLIQ::getLiquidBytes(uint32 value)
{
    uint8 byte;
    if (value < getLiquidBytesMax())
        memcpy(&byte, ((uint8*)this + sizeof(WMOLiquidHeader) + getLiqVertSize() + value*sizeof(uint8)), sizeof(uint8));
    return byte;
}

