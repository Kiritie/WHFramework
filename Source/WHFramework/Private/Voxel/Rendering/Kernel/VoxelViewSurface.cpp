#include "Voxel/Rendering/Kernel/VoxelViewSurface.h"
namespace VoxelRepair  {
    SurfaceStatus ExtractSurface(Point origin,int32_t side,const FinalSampler& sample,SurfaceResult& out,std::string& e,uint32_t maxQuads,const std::atomic_bool* cancel) {
        if(Canceled(cancel)) {
            e="Canceled";
            return SurfaceStatus::Canceled;
        }
        if(side<1||side>64||!sample||maxQuads<1||maxQuads>262144||!ValidPoint(origin) ||!ValidPoint( {
            origin.x-1,origin.y-1,origin.z-1
        }) ||!ValidPoint( {
            origin.x+side,origin.y+side,origin.z+side
        })) {
            e="Invalid bounded surface request";
            return SurfaceStatus::Error;
        }
        using PlaneKey=std::tuple<uint8_t,int32_t,Cell,bool>;
        using UV=std::pair<int32_t,int32_t>;
        // v,u order for stable greedy processing
        std::map<PlaneKey,std::set<UV>> planes;
        SurfaceResult result;
        const int surfaceDx[6]= {
            1,-1,0,0,0,0
        },surfaceDy[6]= {
            0,0,1,-1,0,0
        },surfaceDz[6]= {
            0,0,0,0,1,-1
        };
        for(int z=0;z<side;++z)for(int y=0;y<side;++y)for(int x=0;x<side;++x) {
            if(Canceled(cancel)) {
                e="Canceled";
                return SurfaceStatus::Canceled;
            }
            Point p {
                origin.x+x,origin.y+y,origin.z+z
            };
            ReadBlock a=sample(p);
            if(a.state!=ReadState::Known) {
                e=a.state==ReadState::Error?"Final data error":"Pending current data";
                return a.state==ReadState::Error?SurfaceStatus::Error:SurfaceStatus::Pending;
            }
            if(!a.value)continue;
            for(uint8_t f=0;f<6;++f) {
                Point n {
                    p.x+surfaceDx[f],p.y+surfaceDy[f],p.z+surfaceDz[f]
                };
                auto decision=DecideFace(a,sample(n));
                if(decision==FaceDecision::Deferred) {
                    e="Pending authoritative halo; displayed topology is not a substitute";
                    return SurfaceStatus::Pending;
                }
                if(decision==FaceDecision::Error) {
                    e="Final data error or special shape requires the shape path";
                    return SurfaceStatus::Error;
                }
                if(decision!=FaceDecision::Emit)continue;
                const int coords[3]= {
                    p.x,p.y,p.z
                };
                const int axis=f/2,u=(axis+1)%3,v=(axis+2)%3;
                const int plane=coords[axis]+((f&1)?0:1);
                if(result.exposedUnitFaces>=262144) {
                    e="Unit-surface budget exceeded: split the read tile";
                    return SurfaceStatus::Split;
                }
                planes[ {
                    f,plane,a.value,a.fluid
                }
                ].insert( {
                    coords[v],coords[u]
                });
                ++result.exposedUnitFaces;
            }
        }
        for(auto& plane:planes) {
            auto& mask=plane.second;
            while(!mask.empty()) {
                if(Canceled(cancel)) {
                    e="Canceled";
                    return SurfaceStatus::Canceled;
                }
                auto first=*mask.begin();
                const int v=first.first,u=first.second;
                int width=1,height=1;
                while(mask.count( {
                    v,u+width
                }))++width;
                bool grow=true;
                while(grow) {
                    for(int du=0;du<width;++du)if(!mask.count( {
                        v+height,u+du
                    })) {
                        grow=false;
                        break;
                    }
                    if(grow)++height;
                }
                if(result.quads.size()>=maxQuads) {
                    e="Surface budget exceeded: split, do not truncate";
                    return SurfaceStatus::Split;
                }
                result.quads.push_back( {
                    std::get<0>(plane.first),std::get<1>(plane.first),u,v,width,height,std::get<2>(plane.first),std::get<3>(plane.first)
                });
                for(int dv=0;dv<height;++dv)for(int du=0;du<width;++du)mask.erase( {
                    v+dv,u+du
                });
            }
        }
        out=std::move(result);
        e.clear();
        return SurfaceStatus::Success;
    }
}
