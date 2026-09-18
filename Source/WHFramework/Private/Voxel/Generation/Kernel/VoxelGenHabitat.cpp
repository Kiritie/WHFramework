#include "Voxel/Generation/Kernel/VoxelGenHabitat.h"
namespace VoxelRepair  {
    bool ResolveHabitat(const Environment& in,Ecology& out,std::string& e) {
        if(in.temperatureQ15<-32768||in.temperatureQ15>32767||in.moistureQ15<-32768||in.moistureQ15>32767 ||in.slopePermille<0||in.slopePermille>100000||in.snowLineCm<1000||in.snowLineCm>800000) {
            e="Invalid habitat sample";
            return false;
        }
        Ecology r;
        if(in.underWater)r= {
            Habitat::Ocean,0,0
        };
        else if(in.slopePermille>1200)r= {
            Habitat::BareRock,0,8
        };
        else if(in.altitudeCm>=in.snowLineCm||in.temperatureQ15<-22000)r= {
            Habitat::Snow,0,5
        };
        else if(int64_t(in.altitudeCm)>int64_t(in.snowLineCm)-3000)r= {
            Habitat::Alpine,20,100
        };
        else if(in.temperatureQ15>9000&&in.moistureQ15<-5000)r= {
            Habitat::Desert,0,10
        };
        else if(in.nearWater&&in.moistureQ15>10000)r= {
            Habitat::Wetland,160,320
        };
        else if(in.temperatureQ15<-5000)r= {
            Habitat::ConiferForest,650,180
        };
        else if(in.moistureQ15>2500)r= {
            Habitat::TemperateForest,700,260
        };
        else r= {
            Habitat::Grassland,130,300
        };
        const int32_t slopeFactor=std::clamp(1000-in.slopePermille,0,1000);
        r.treePermille=uint16_t(int32_t(r.treePermille)*slopeFactor/1000);
        // Altitude never unconditionally removes all vegetation below the declared tree line.
        out=r;
        e.clear();
        return true;
    }
    bool ValidateDefaultContent(const ContentInventory& c,std::string& e) {
        if((c.enabled&RequiredContent)!=RequiredContent) {
            e="A required default-world generator is disabled";
            return false;
        }
        if(c.treeRules<2||c.plantRules<3||c.oreRules<3||!c.castles||!c.dungeons||!c.townHouses||!c.details) {
            e="Default profile content inventory is incomplete";
            return false;
        }
        if((c.witnessed&RequiredContent)!=RequiredContent) {
            e="Candidates are not final generated content witnesses";
            return false;
        }
        e.clear();
        return true;
    }
}
