#include "Voxel/Rendering/Kernel/VoxelViewPriority.h"
namespace VoxelRepair  {
    WorkRank RankWork(const WorkItem& w,uint64_t now) {
        const uint64_t age=now>w.queuedMs?std::min<uint64_t>(60000,now-w.queuedMs):0;
        uint8_t group=uint8_t(w.kind);
        if(age>=30000&&group>uint8_t(WorkClass::VisibleSurface))group=uint8_t(WorkClass::VisibleSurface);
        int64_t distance=std::clamp<int64_t>(w.distanceSquaredCells,0,int64_t {
            1
        }
        <<50);
        int64_t score=distance-int64_t(age)*4-int64_t(std::min<uint32_t>(w.forwardScore,1000))*64;
        return  {
            group,score,w.stableId
        };
    }
    std::vector<uint64_t> OrderMeshWork(const std::vector<WorkItem>& work,uint64_t now) {
        std::vector<std::pair<WorkRank,uint64_t>> ranks;
        for(const auto& w:work)if(!w.certifiedEmpty)ranks.push_back( {
            RankWork(w,now),w.stableId
        });
        std::sort(ranks.begin(),ranks.end(),[](const auto& a,const auto& b) {
            return a.first<b.first;
        });
        std::vector<uint64_t> out;
        for(const auto& r:ranks)out.push_back(r.second);
        return out;
    }
}
