#include "Voxel/Generation/Kernel/VoxelGenMath.h"
namespace VoxelGen
{
int32_t FloorDiv(int32_t v,int32_t d) { const int32_t q=v/d,r=v%d;return q-(r<0); }
int32_t CeilDivPositive(int32_t v,int32_t d) { return int32_t((int64_t(v)+d-1)/d); }
uint64_t Mix(uint64_t v) { v+=0x9e3779b97f4a7c15ull;v=(v^(v>>30))*0xbf58476d1ce4e5b9ull;v=(v^(v>>27))*0x94d049bb133111ebull;return v^(v>>31); }
uint64_t Seed(int32_t w,I3 a,uint64_t salt) { uint64_t h=Mix(uint32_t(w));h=Mix(h^uint32_t(a.x));h=Mix(h^uint32_t(a.y));h=Mix(h^uint32_t(a.z));return Mix(h^salt); }
Id MakeId(int32_t s,I3 a,uint64_t k,uint64_t n) { const uint64_t h=Seed(s,a,k);return {h,Mix(h^n)}; }
int32_t Range(uint64_t s,int32_t a,int32_t b) { return a+int32_t(s%uint64_t(int64_t(b)-a+1)); }
static int64_t Lerp(int64_t a,int64_t b,int32_t t) { return a+(b-a)*t/65536; }
static int32_t Fraction(int32_t p,int32_t cell,int32_t period) { int64_t t=(int64_t(p)-int64_t(cell)*period)*65536/period;return int32_t((t*t/65536)*(196608-2*t)/65536); }
int32_t Noise(int32_t seed,I3 p,int32_t period,uint64_t salt)
{
    const I3 a{FloorDiv(p.x,period),FloorDiv(p.y,period),FloorDiv(p.z,period)};
    const int32_t tx=Fraction(p.x,a.x,period),ty=Fraction(p.y,a.y,period),tz=Fraction(p.z,a.z,period);
    int64_t z[2];
    for(int k=0;k<2;++k) { int64_t y[2];for(int j=0;j<2;++j) { const int64_t x0=int64_t(Seed(seed,{a.x,a.y+j,a.z+k},salt)&65535)-32768; const int64_t x1=int64_t(Seed(seed,{a.x+1,a.y+j,a.z+k},salt)&65535)-32768;y[j]=Lerp(x0,x1,tx); } z[k]=Lerp(y[0],y[1],ty); }
    return int32_t(Lerp(z[0],z[1],tz));
}
int32_t Noise2(int32_t s,int32_t x,int32_t y,int32_t p,uint64_t salt) { return Noise(s,{x,y,0},p,salt); }
int64_t Dot(I3 a,I3 b) { return int64_t(a.x)*b.x+int64_t(a.y)*b.y+int64_t(a.z)*b.z; }
I3 RotateCell(I3 p,uint8_t yaw) { for(uint8_t i=0;i<(yaw&3);++i) p={-p.y-1,p.x,p.z};return p; }
I3 RotateCorner(I3 p,uint8_t yaw) { for(uint8_t i=0;i<(yaw&3);++i) p={-p.y,p.x,p.z};return p; }
Box TransformBox(Box b,I3 o,int32_t s,uint8_t y)
{
    Box out{{INT32_MAX,INT32_MAX,INT32_MAX},{INT32_MIN,INT32_MIN,INT32_MIN}};
    for(int i=0;i<8;++i) { I3 p{(i&1?b.max.x:b.min.x)*s,(i&2?b.max.y:b.min.y)*s,(i&4?b.max.z:b.min.z)*s};p=RotateCorner(p,y)+o;out.min.x=std::min(out.min.x,p.x);out.min.y=std::min(out.min.y,p.y);out.min.z=std::min(out.min.z,p.z);out.max.x=std::max(out.max.x,p.x);out.max.y=std::max(out.max.y,p.y);out.max.z=std::max(out.max.z,p.z); }
    return out;
}
bool InEllipsoid(I3 p,I3 c,I3 r)
{
    // Twice-cell-center coordinates keep all occupancy decisions integral.
    const int64_t x=2ll*(p.x-c.x)+1,y=2ll*(p.y-c.y)+1,z=2ll*(p.z-c.z)+1;
    if(r.x<=0||r.y<=0||r.z<=0||x<-2ll*r.x||x>2ll*r.x||y<-2ll*r.y||y>2ll*r.y||z<-2ll*r.z||z>2ll*r.z) return false;
    constexpr int64_t q=1ll<<24;
    return x*x*q/(4ll*r.x*r.x)+y*y*q/(4ll*r.y*r.y)+z*z*q/(4ll*r.z*r.z)<=q;
}
bool InCapsule(I3 p,I3 a,I3 b,int32_t r)
{
    const I3 d=b-a,v=p-a;const int64_t len=Dot(d,d),t=Dot(v,d),rr=int64_t(r)*r;
    if(!len||t<=0) return Dot(v,v)<=rr;
    if(t>=len) return Dot(p-b,p-b)<=rr;
    return Dot(v,v)*len-t*t<=rr*len;
}
uint64_t HashBytes(const std::vector<uint8_t>& b) { uint64_t h=14695981039346656037ull;for(uint8_t v:b){h^=v;h*=1099511628211ull;}return h; }
}
