#include "Voxel/Generation/Kernel/VoxelGenCaveRoutes.h"
#include <deque>
namespace VoxelRepair  {
    bool CaveRoute::Carves(Point p)const {
        auto it=footprint.find( {
            p.x,p.y
        });
        return it!=footprint.end()&&p.z>=it->second.foot&&p.z<it->second.ceiling;
    }
    bool CaveRoute::ProtectsFloor(Point p)const {
        auto it=footprint.find( {
            p.x,p.y
        });
        return it!=footprint.end()&&p.z==it->second.foot-1;
    }
    bool ValidateWalkRoute(const CaveRoute& r,Point start,Point end,std::string& e,const std::atomic_bool* cancel) {
        if(Canceled(cancel)) {
            e="Canceled";
            return false;
        }
        using XY=std::pair<int32_t,int32_t>;
        XY a {
            start.x,start.y
        },b {
            end.x,end.y
        };
        if(r.footprint.empty()||r.footprint.size()>262144||r.minClearHeight<1||r.maxStepCells<0 ||!r.footprint.count(a)||!r.footprint.count(b)) {
            e="Invalid cave walk footprint";
            return false;
        }
        for(const auto& p:r.footprint)if(int64_t(p.second.ceiling)-p.second.foot<r.minClearHeight) {
            e="Insufficient cave headroom";
            return false;
        }
        std::set<XY> seen;
        std::deque<XY> q;
        q.push_back(a);
        seen.insert(a);
        const int dx[4]= {
            1,-1,0,0
        },dy[4]= {
            0,0,1,-1
        };
        while(!q.empty()) {
            if(Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            XY x=q.front();
            q.pop_front();
            if(x==b) {
                e.clear();
                return true;
            }
            auto s=r.footprint.at(x);
            for(int i=0;i<4;++i) {
                XY n {
                    x.first+dx[i],x.second+dy[i]
                };
                auto it=r.footprint.find(n);
                if(it==r.footprint.end()||seen.count(n)||std::abs(int64_t(it->second.foot)-s.foot)>r.maxStepCells)continue;
                if(int64_t(std::min(s.ceiling,it->second.ceiling))-std::max(s.foot,it->second.foot)<r.minClearHeight)continue;
                seen.insert(n);
                q.push_back(n);
            }
        }
        e="Cave floor is not walk-connected";
        return false;
    }
    bool BuildCaveRoute(uint64_t seed,Point a,Point b,const CaveRouteSettings& s,CaveRoute& out,std::string& e,const std::atomic_bool* cancel) {
        if(Canceled(cancel)) {
            e="Canceled";
            return false;
        }
        const int64_t dx=std::abs(int64_t(a.x)-b.x),dy=std::abs(int64_t(a.y)-b.y),length=dx+dy,drop=int64_t(a.z)-b.z;
        if(!ValidPoint(a)||!ValidPoint(b)||length<1||length+1>s.maxNodes||s.maxNodes>4096 ||s.radiusCells<2||s.radiusCells>16||s.clearHeight<2||s.clearHeight>64||s.requiredHeadroom<2||s.requiredHeadroom>s.clearHeight-s.maxStepCells||s.maxStepCells<1 ||s.maxStepCells>2||s.minHorizontalPerDrop<2||s.minHorizontalPerDrop>16 ||drop<0||drop*s.minHorizontalPerDrop>length||s.maxFootprint>262144||!s.maxFootprint) {
            e="Cave route cannot meet bounded slope/size";
            return false;
        }
        CaveRoute r;
        r.minClearHeight=s.requiredHeadroom;
        r.maxStepCells=s.maxStepCells;
        r.centerline.push_back(a);
        Point p=a;
        int64_t leftX=dx,leftY=dy;
        for(int64_t i=1;i<=length;++i) {
            if(Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            bool x=leftX>0&&(leftY==0||Mix(seed+uint64_t(i))%uint64_t(leftX+leftY)<uint64_t(leftX));
            if(x) {
                p.x+=b.x>a.x?1:-1;
                --leftX;
            }
            else {
                p.y+=b.y>a.y?1:-1;
                --leftY;
            }
            p.z=a.z-int32_t(i*drop/length);
            r.centerline.push_back(p);
        }
        for(Point node:r.centerline)for(int y=-s.radiusCells;y<=s.radiusCells;++y)for(int x=-s.radiusCells;x<=s.radiusCells;++x) {
            if(Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            if(x*x+y*y>s.radiusCells*s.radiusCells)continue;
            Point cell {
                node.x+x,node.y+y,node.z
            };
            if(!ValidPoint(cell)||cell.z-1<-32768||int64_t(cell.z)+s.clearHeight>32768) {
                e="Cave reaches world coordinate bounds";
                return false;
            }
            auto key=std::make_pair(cell.x,cell.y);
            auto it=r.footprint.find(key);
            if(it==r.footprint.end()) {
                if(r.footprint.size()>=s.maxFootprint) {
                    e="Cave raster footprint budget exceeded";
                    return false;
                }
                r.footprint.emplace(key,WalkSpan {
                    cell.z,cell.z+s.clearHeight
                });
            }
            else {
                it->second.foot=std::min(it->second.foot,cell.z);
                it->second.ceiling=std::max(it->second.ceiling,cell.z+s.clearHeight);
            }
        }
        if(!ValidateWalkRoute(r,a,b,e,cancel))return false;
        out=std::move(r);
        e.clear();
        return true;
    }
    bool AddCaveBranch(const CaveRoute& branch,CaveRoute& system,std::string& e,const std::atomic_bool* cancel) {
        if(Canceled(cancel)) {
            e="Canceled";
            return false;
        }
        if(branch.centerline.empty()||system.centerline.empty()||branch.footprint.empty() ||branch.minClearHeight!=system.minClearHeight||branch.maxStepCells!=system.maxStepCells) {
            e="Incompatible cave branch";
            return false;
        }
        auto first=branch.centerline.front();
        if(!system.Carves(first)) {
            e="Branch start is not connected to the main cave";
            return false;
        }
        CaveRoute r=system;
        for(const auto& p:branch.footprint) {
            if(Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            auto it=r.footprint.find(p.first);
            if(it==r.footprint.end()) {
                if(r.footprint.size()>=262144) {
                    e="Cave system footprint budget exceeded";
                    return false;
                }
                r.footprint.emplace(p);
            }
            else {
                it->second.foot=std::min(it->second.foot,p.second.foot);
                it->second.ceiling=std::max(it->second.ceiling,p.second.ceiling);
            }
        }
        if(!ValidateWalkRoute(r,system.centerline.front(),system.centerline.back(),e,cancel) ||!ValidateWalkRoute(r,system.centerline.front(),branch.centerline.back(),e,cancel))return false;
        // The system must store branch polylines separately in its immutable production plan.
        system=std::move(r);
        e.clear();
        return true;
    }
}
