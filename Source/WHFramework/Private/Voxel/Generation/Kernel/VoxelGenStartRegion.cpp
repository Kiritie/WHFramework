#include "Voxel/Generation/Kernel/VoxelGenStartRegion.h"
namespace VoxelRepair  {
    namespace  {
        Point Rotate(Point p,uint8_t yaw) {
            for(uint8_t i=0;i<(yaw&3);++i) {
                int32_t x=p.x;
                p.x=-p.y;
                p.y=x;
            }
            return p;
        }
    }
    bool BuildStartRegion(int32_t seed,Point origin,int32_t size,int32_t ground,StartRegionPlan& out,std::string& e) {
        if(!ValidPoint(origin)||(size!=20&&size!=25)||ground<-30000||ground>30000) {
            e="Invalid start-region units/origin";
            return false;
        }
        StartRegionPlan p;
        p.layoutSeed=Mix(uint32_t(seed)^0x5354415254ULL);
        p.origin=origin;
        p.cellCm=size;
        p.coreCells=80000/size;
        p.outerCells=120000/size;
        p.valleyPlane=ground;
        p.yaw=uint8_t(p.layoutSeed&3);
        // These are planned sockets, not runtime block writes. Z is a relative target floor offset.
        const std::pair<StartAnchorKind,Point> cm[]= {
            {
                StartAnchorKind::Spawn, {
                    0,0,0
                }
            }, {
                StartAnchorKind::Town, {
                    9600,0,0
                }
            },  {
                StartAnchorKind::Lake, {
                    -9600,6400,-300
                }
            }, {
                StartAnchorKind::RiverSource, {
                    20000,16000,1600
                }
            },  {
                StartAnchorKind::CaveEntrance, {
                    16000,-6400,400
                }
            }, {
                StartAnchorKind::Forest, {
                    0,12800,0
                }
            },  {
                StartAnchorKind::Castle, {
                    -60000,-16000,1200
                }
            }, {
                StartAnchorKind::Dungeon, {
                    44800,51200,-1600
                }
            },  {
                StartAnchorKind::LavaChamber, {
                    20000,-10000,-12000
                }
            }
        };
        for(auto a:cm) {
            Point v {
                a.second.x/size,a.second.y/size,a.second.z/size
            };
            p.anchors.push_back( {
                a.first,Rotate(v,p.yaw)
            });
        }
        for(int sign: {
            -1,1
        })if(!ValidPoint( {
            origin.x+sign*p.outerCells,origin.y+sign*p.outerCells,origin.z
        })) {
            e="Start-region reaches world coordinate limit";
            return false;
        }
        out=std::move(p);
        e.clear();
        return true;
    }
    bool StartRegionPlan::ConstrainHeight(Point xy,int32_t raw,int32_t& out,std::string& e)const {
        if(!ValidPoint(xy)||coreCells<=0||outerCells<=coreCells||cellCm<=0||raw<-32768||raw>32768) {
            e="Invalid terrain constraint";
            return false;
        }
        Point d {
            xy.x-origin.x,xy.y-origin.y,0
        };
        d=Rotate(d,uint8_t((4-yaw)&3));
        const int64_t r=std::max(std::abs(int64_t(d.x)),std::abs(int64_t(d.y)));
        if(r>=outerCells) {
            out=raw;
            e.clear();
            return true;
        }
        // Broad valley with sides rising away from its central corridor. All calculations are integral.
        const int64_t shoulder=std::max<int64_t>(0,std::abs(int64_t(d.y))-12800/cellCm);
        const int64_t longitudinal=std::abs(int64_t(d.x))/48;
        const int64_t target=int64_t(valleyPlane)+shoulder/8+longitudinal;
        int32_t weight=r<=coreCells?65536:65536-SmoothQ16(int32_t((r-coreCells)*65536/(outerCells-coreCells)));
        int64_t value=(int64_t(raw)*(65536-weight)+target*weight)/65536;
        if(value<-32768||value>32768) {
            e="Constrained height exceeds world bounds";
            return false;
        }
        out=int32_t(value);
        e.clear();
        return true;
    }
}
