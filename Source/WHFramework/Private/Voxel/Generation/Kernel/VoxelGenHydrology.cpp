#include "Voxel/Generation/Kernel/VoxelGenHydrology.h"
#include <queue>
namespace VoxelRepair  {
    namespace  {
        bool Count(uint32_t w,uint32_t h,size_t& n)  {
            if(w<3||h<3||w>257||h>257)return false;
            n=size_t(w)*h;
            return true;
        }
        const int dx[4]= {
            1,-1,0,0
        },dy[4]= {
            0,0,1,-1
        };
        bool Neighbor(uint32_t i,uint8_t f,uint32_t w,uint32_t h,uint32_t& j)  {
            int x=int(i%w)+dx[f],y=int(i/w)+dy[f];
            if(x<0||y<0||x>=int(w)||y>=int(h))return false;
            j=uint32_t(x+y*int(w));
            return true;
        }
    }
    bool BuildDrainage(const DrainageInput& in,DrainageResult& out,std::string& e,const std::atomic_bool* cancel)  {
        if(Canceled(cancel)) {
            e="Canceled";
            return false;
        }
        size_t n=0;
        if(!Count(in.width,in.height,n)||in.groundPlane.size()!=n||in.rainWeight.size()!=n ||in.allowed.size()!=n||in.outlets.empty()) {
            e="Invalid drainage arrays/outlets";
            return false;
        }
        for(size_t i=0;i<n;++i)if(in.groundPlane[i]<-32768||in.groundPlane[i]>32768 ||in.rainWeight[i]>10000||in.allowed[i]>1) {
            e="Drainage value outside bounds";
            return false;
        }
        const int32_t inf=std::numeric_limits<int32_t>::max();
        DrainageResult r;
        r.spillPlane.assign(n,inf);
        r.parent.assign(n,-2);
        r.accumulation.assign(n,0);
        std::vector<uint8_t> settled(n,0),terminal(n,0);
        using Entry=std::pair<int32_t,uint32_t>;
        std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> q;
        for(auto item:in.outlets) {
            uint32_t i=item.first;
            int32_t water=item.second;
            if(i>=n||!in.allowed[i]||water<-32768||water>32768) {
                e="Invalid outlet";
                return false;
            }
            terminal[i]=1;
            r.spillPlane[i]=std::max(in.groundPlane[i],water);
            r.parent[i]=-1;
            q.emplace(r.spillPlane[i],i);
        }
        while(!q.empty()) {
            if(Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            auto v=q.top();
            q.pop();
            uint32_t i=v.second;
            if(settled[i]||v.first!=r.spillPlane[i])continue;
            settled[i]=1;
            r.settledOrder.push_back(i);
            r.accumulation[i]=in.rainWeight[i];
            for(uint8_t f=0;f<4;++f) {
                uint32_t j;
                if(!Neighbor(i,f,in.width,in.height,j)||!in.allowed[j]||settled[j]||terminal[j])continue;
                int32_t candidate=std::max(v.first,in.groundPlane[j]);
                if(candidate<r.spillPlane[j]) {
                    r.spillPlane[j]=candidate;
                    r.parent[j]=int32_t(i);
                    q.emplace(candidate,j);
                }
                // Equal candidates retain the first settled predecessor: deterministic and acyclic.
            }
        }
        for(size_t i=0;i<n;++i)if(in.allowed[i]&&!settled[i]) {
            e="Allowed drainage component has no certified outlet";
            return false;
        }
        for(auto it=r.settledOrder.rbegin();it!=r.settledOrder.rend();++it) {
            int32_t p=r.parent[*it];
            if(p>=0)r.accumulation[size_t(p)]+=r.accumulation[*it];
        }
        out=std::move(r);
        e.clear();
        return true;
    }
    bool TraceDrainage(const DrainageInput& in,const DrainageResult& r,uint32_t source, std::vector<uint32_t>& out,std::string& e) {
        size_t n=0;
        if(!Count(in.width,in.height,n)||source>=n||in.allowed.size()!=n ||!in.allowed[source]||r.parent.size()!=n||r.spillPlane.size()!=n) {
            e="Invalid route request";
            return false;
        }
        std::vector<uint32_t> path;
        std::vector<uint8_t> seen(n,0);
        uint32_t i=source;
        while(true) {
            if(i>=n||seen[i]||path.size()>=4096) {
                e="Drainage cycle or route budget exceeded";
                return false;
            }
            seen[i]=1;
            path.push_back(i);
            int32_t p=r.parent[i];
            if(p==-1) {
                if(!in.outlets.count(i)) {
                    e="Uncertified route terminal";
                    return false;
                }
                break;
            }
            if(p<0||size_t(p)>=n||r.spillPlane[size_t(p)]>r.spillPlane[i]) {
                e="Invalid upstream parent";
                return false;
            }
            uint32_t px=uint32_t(p)%in.width,py=uint32_t(p)/in.width;
            if(std::abs(int(px)-int(i%in.width))+std::abs(int(py)-int(i/in.width))!=1) {
                e="Nonadjacent drainage parent";
                return false;
            }
            i=uint32_t(p);
        }
        out=std::move(path);
        e.clear();
        return true;
    }
    bool ValidateBasin(const BasinInput& in,BasinCertificate& out,std::string& e,const std::atomic_bool* cancel) {
        if(Canceled(cancel)) {
            e="Canceled";
            return false;
        }
        size_t n=0;
        if(!Count(in.width,in.height,n)||in.groundPlane.size()!=n||in.known.size()!=n ||in.waterMask.size()!=n||in.waterPlane<-32768||in.waterPlane>32768) {
            e="Invalid basin arrays";
            return false;
        }
        for(auto p:in.connectorPlanes)if(p.first.first>=n||p.first.second>=4||p.second!=in.waterPlane) {
            e="Invalid/mismatched water connector";
            return false;
        }
        BasinCertificate r;
        uint32_t first=0;
        for(uint32_t i=0;i<n;++i) {
            if((i&255)==0&&Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            if(in.known[i]>1||in.waterMask[i]>1) {
                e="Invalid basin flags";
                return false;
            }
            if(!in.waterMask[i])continue;
            first=i;
            ++r.waterCells;
            if(!in.known[i]||in.groundPlane[i]>=in.waterPlane) {
                e="Unknown or dry cell in water footprint";
                return false;
            }
            for(uint8_t f=0;f<4;++f) {
                uint32_t j=0;
                bool has=Neighbor(i,f,in.width,in.height,j);
                if(has&&in.waterMask[j])continue;
                if(has&&in.known[j]&&in.groundPlane[j]>=in.waterPlane) {
                    ++r.closedEdges;
                    continue;
                }
                auto it=in.connectorPlanes.find( {
                    i,f
                });
                if(it!=in.connectorPlanes.end()) {
                    // The caller must have validated the receiving body separately at this exact plane.
                    ++r.connectorEdges;
                    continue;
                }
                e=has&&!in.known[j]?"Unknown shore cannot certify a lake":"Lake footprint leaks through final shore";
                return false;
            }
        }
        if(!r.waterCells) {
            e="Empty basin is not a lake certificate";
            return false;
        }
        std::vector<uint8_t> seen(n,0);
        std::vector<uint32_t> queue {
            first
        };
        seen[first]=1;
        for(size_t at=0;at<queue.size();++at) {
            if((at&255)==0&&Canceled(cancel)) {
                e="Canceled";
                return false;
            }
            uint32_t i=queue[at];
            for(uint8_t f=0;f<4;++f) {
                uint32_t j;
                if(Neighbor(i,f,in.width,in.height,j)&&in.waterMask[j]&&!seen[j]) {
                    seen[j]=1;
                    queue.push_back(j);
                }
            }
        }
        if(queue.size()!=r.waterCells) {
            e="Disconnected footprint must be separate lake bodies";
            return false;
        }
        out=r;
        e.clear();
        return true;
    }
    bool EvaluateRiverSection(int32_t original,int32_t water,int32_t d,const RiverShape& s, RiverSection& out,std::string& e) {
        if(original<-32768||original>32768||water<-32768||water>32767||d<0||d>65536 ||s.halfWidth<1||s.halfWidth>256||s.bankWidth<1||s.bankWidth>512 ||s.shoreWidth<1||s.shoreWidth>1024||s.depth<1||s.depth>256 ||s.maxCutFill<1||s.maxCutFill>512||water-s.depth<-32768) {
            e="Invalid river section";
            return false;
        }
        RiverSection r;
        r.waterPlane=water;
        r.bedPlane=water-s.depth;
        if(d<=s.halfWidth)r.groundPlane=r.bedPlane;
        else if(d<=s.halfWidth+s.bankWidth) {
            int32_t t=int32_t(int64_t(d-s.halfWidth)*65536/s.bankWidth);
            r.groundPlane=r.bedPlane+int32_t(int64_t(s.depth+1)*SmoothQ16(t)/65536);
        }
        else if(d<s.halfWidth+s.bankWidth+s.shoreWidth) {
            int32_t t=int32_t(int64_t(d-s.halfWidth-s.bankWidth)*65536/s.shoreWidth);
            r.groundPlane=water+1+int32_t((int64_t(original)-water-1)*SmoothQ16(t)/65536);
        }
        else r.groundPlane=original;
        if(std::abs(int64_t(r.groundPlane)-original)>s.maxCutFill) {
            e="River route exceeds cut/fill budget";
            return false;
        }
        r.wet=d<=s.halfWidth+s.bankWidth&&r.groundPlane<water;
        out=r;
        e.clear();
        return true;
    }
}
