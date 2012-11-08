#ifndef WMO_H
#define WMO_H


class WMOInstance
{
        static std::set<int> ids;
    public:
        std::string MapName;
        int currx;
        int curry;
        WMOGroup *wmo;
        Vec3D pos;
        Vec3D pos2, pos3, rot;
        uint32 indx,id, d2, d3;
        int doodadset;

        WMOInstance(MPQFile &f,const char* WmoInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE *pDirfile);

        static void reset();
};

#endif
