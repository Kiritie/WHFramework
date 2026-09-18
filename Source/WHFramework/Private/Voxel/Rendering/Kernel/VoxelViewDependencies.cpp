#include "Voxel/Rendering/Kernel/VoxelViewDependencies.h"
namespace VoxelRepair  {
    FaceDecision DecideFace(const ReadBlock& a,const ReadBlock& b) {
        if(a.state==ReadState::Error)return FaceDecision::Error;
        if(a.state!=ReadState::Known)return FaceDecision::Deferred;
        if(!a.value)return FaceDecision::Hidden;
        if(a.specialShape)return FaceDecision::Error;
        // Route to the existing Shape coverage path, never silently fill a cube.
        if(b.state==ReadState::Error)return FaceDecision::Error;
        if(b.state!=ReadState::Known)return FaceDecision::Deferred;
        if(!b.value)return FaceDecision::Emit;
        if(b.specialShape)return FaceDecision::Error;
        if(b.opaqueFull)return FaceDecision::Hidden;
        if(a.fluid&&b.fluid) {
            if(uint16_t(a.value)==uint16_t(b.value))return FaceDecision::Hidden;
            // One canonical two-sided interface patch for unlike fluids; no two coincident quads.
            return uint16_t(a.value)<uint16_t(b.value)?FaceDecision::Emit:FaceDecision::Hidden;
        }
        return FaceDecision::Emit;
    }
    bool CanPublish(const DependencyStamp& a,const DependencyStamp& b,const std::vector<DependencyStamp>& na,const std::vector<DependencyStamp>& nb) {
        if(!a.epoch||!a.recipe||!(a==b)||na.size()!=nb.size()||na.size()>24)return false;
        for(size_t i=0;i<na.size();++i)if(!na[i].epoch||na[i].epoch!=a.epoch||na[i].recipe!=a.recipe||!(na[i]==nb[i]))return false;
        return true;
    }
}
