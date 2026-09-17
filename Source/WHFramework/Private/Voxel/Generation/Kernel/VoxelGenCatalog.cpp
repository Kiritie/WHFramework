#include "Voxel/Generation/Kernel/VoxelGenCatalog.h"
#include "Voxel/Generation/Kernel/VoxelGenMath.h"
#include <set>
namespace VoxelGen
{
bool Settings::Validate(std::string& e) const
{
    if(cellCm<10||cellCm>100||minZ< -32768||maxZ>32768||maxZ<=minZ||maxZ-minZ>8192||sea<minZ||sea>=maxZ||base<minZ||base>=maxZ) { e="Invalid voxel/world dimensions";return false; }
    const int32_t periods[]={continentPeriod,erosionPeriod,mountainPeriod,climatePeriod,detailPeriod,riverPeriod,cavePeriod,chamberPeriod};
    for(int32_t p:periods) if(p<2||p>1048576) { e="Noise period out of range";return false; }
    if(continentAmplitude<0||continentAmplitude>2048||mountainAmplitude<0||mountainAmplitude>2048||detailAmplitude<0||detailAmplitude>64||riverWidthQ15<0||riverWidthQ15>8192||riverDepth<1||riverDepth>64||lakeSpacing<64||lakeSpacing>8192||lakeRadius<8||lakeRadius>lakeSpacing/4||lakeDepth<1||lakeDepth>64||caveWidthQ15<1||caveWidthQ15>8192||chamberThresholdQ15<8192||chamberThresholdQ15>32767||aquiferSpacing<32||aquiferSpacing>512||aquiferRadius<4||aquiferRadius>aquiferSpacing/4||lavaCeiling<minZ||lavaCeiling>=sea||structureSpacing<3072||structureSpacing>16384||structureChancePermille<0||structureChancePermille>1000||maxSiteCutFill<1||maxSiteCutFill>64) {e="Generation setting outside bounded implementation";return false;}
    e.clear();return true;
}
Cell StructureTemplate::Sample(I3 p,bool& owns) const
{
    owns=false;
    if(!bounds.Contains(p)) return 0;
    // Every run has one Y/Z row. The validated sort order is Z,Y,X.
    auto it=std::upper_bound(writes.begin(),writes.end(),p,[](I3 q,const Run& r){return q<r.start;});
    if(it!=writes.begin()) {--it;if(it->start.y==p.y&&it->start.z==p.z&&p.x>=it->start.x&&int64_t(p.x)<int64_t(it->start.x)+it->length){owns=true;return it->value;} }
    for(const auto& b:clear) if(b.Contains(p)){owns=true;return 0;}
    return 0;
}
bool Catalog::Validate(const Settings& s,std::string& e) const
{
    if(!s.Validate(e)) return false;
    if(blocks.size()<10||blocks.size()>65535||blocks[0]!="core:air"||trees.size()>32||plants.size()>64||ores.size()>32||structures.size()>256||details.size()>64){e="Catalog cardinality invalid";return false;}
    std::set<std::string> names;
    for(const auto& n:blocks) if(n.empty()||n.size()>256||!names.insert(n).second){e="Duplicate/empty block name";return false;}
    auto block=[&](uint16_t v){return v>0&&v<blocks.size();};
    const uint16_t required[]={palette.stone,palette.dirt,palette.grass,palette.sand,palette.snow,palette.water,palette.lava,palette.bedrock,palette.road};
    for(uint16_t b:required) if(!block(b)){e="Required terrain role is invalid";return false;}
    names.clear();
    for(const auto& t:trees) if(t.key.empty()||!names.insert(t.key).second||uint8_t(t.kind)>1||!block(t.trunk)||!block(t.leaves)||t.spacing<16||t.spacing>512||t.minHeight<8||t.maxHeight<t.minHeight||t.maxHeight>256||t.trunkRadius<1||t.trunkRadius>16||t.crownRadius<4||t.crownRadius>64||t.chancePermille<0||t.chancePermille>1000||t.maxSlope<0||t.maxSlope>64){e="Invalid tree rule";return false;}
    if(!trees.empty())for(const auto& t:trees)if(t.spacing!=trees[0].spacing){e="All trees share one candidate grid spacing";return false;}
    names.clear();
    for(const auto& p:plants) if(p.key.empty()||!names.insert(p.key).second||!block(p.block)||p.chancePermille<0||p.chancePermille>1000||p.patchPeriod<4||p.patchPeriod>1024){e="Invalid plant rule";return false;}
    names.clear();
    for(const auto& o:ores) if(o.key.empty()||!names.insert(o.key).second||!block(o.block)||o.minZ< s.minZ||o.maxZ>s.maxZ||o.minZ>=o.maxZ||o.spacing<16||o.spacing>128||o.radius<1||o.radius>8||o.length<1||o.length>32||o.chancePermille<0||o.chancePermille>1000){e="Invalid ore rule";return false;}
    names.clear();uint64_t totalRuns=0;
    for(const auto& t:structures)
    {
        const int32_t k=t.sourceCellCm/s.cellCm;
        if(t.key.empty()||!names.insert(t.key).second||uint8_t(t.kind)>2||t.sourceCellCm<s.cellCm||t.sourceCellCm%s.cellCm||k>8||!t.bounds.Valid()||t.writes.size()>1048576||t.clear.size()>4096||t.details.size()>64){e="Invalid structure/unsupported source voxel scale";return false;}
        const int64_t dx=int64_t(t.bounds.max.x)-t.bounds.min.x,dy=int64_t(t.bounds.max.y)-t.bounds.min.y,dz=int64_t(t.bounds.max.z)-t.bounds.min.z;
        if(dx*k>768||dy*k>768||dz*k>1024||t.bounds.min.x< -4096||t.bounds.min.y< -4096||t.bounds.min.z< -4096||t.bounds.max.x>4096||t.bounds.max.y>4096||t.bounds.max.z>4096||t.groundZ<t.bounds.min.z||t.groundZ>=t.bounds.max.z){e="Structure bounds exceed query budget";return false;}
        if(!t.bounds.Contains(t.entrance)||t.entrance.z!=t.groundZ+1||(t.kind==StructureKind::TownHouse&&t.entrance.y!=t.bounds.min.y)){e="Explicit entrance must be at ground+1 and TownHouse -Y edge";return false;}
        if(t.kind==StructureKind::Dungeon&&(dz*k>192||t.clear.empty())){e="Dungeon requires clear volumes and height <=192 target cells";return false;}
        if(t.kind==StructureKind::TownHouse&&(dx*k>64||dy*k>80||dz*k>128)){e="TownHouse exceeds its fixed lot; split/reauthor the asset";return false;}
        Run previous;bool first=true;
        for(const auto& r:t.writes)
        {
            if(((Symbol(r.value)!=0&&!block(Symbol(r.value)))||State(r.value)>3||(Symbol(r.value)==0&&State(r.value)!=0))||r.length<=0||!t.bounds.Contains(r.start)||int64_t(r.start.x)+r.length>t.bounds.max.x||(!first&&(!(previous.start<r.start)||(previous.start.y==r.start.y&&previous.start.z==r.start.z&&int64_t(previous.start.x)+previous.length>r.start.x)))){e="Invalid/overlapping/unsorted run or non-cube state in worldgen template";return false;}
            previous=r;first=false;
        }
        for(const Box& b:t.clear) if(!b.Valid()||!t.bounds.Contains(b.min)||b.max.x>t.bounds.max.x||b.max.y>t.bounds.max.y||b.max.z>t.bounds.max.z){e="Clear box outside structure";return false;}
        for(const auto& d:t.details) if(d.asset>=details.size()||d.yaw>3||!t.bounds.Contains(d.position)){e="Invalid detail socket";return false;}
        totalRuns+=t.writes.size();
    }
    if(totalRuns>2097152){e="Total template runs exceed budget";return false;}
    names.clear();for(const auto& d:details) if(d.key.empty()||!names.insert(d.key).second||d.assetPath.empty()||d.assetPath.size()>1024||!d.geometryHash||!d.footprint.Valid()){e="Invalid detail asset fingerprint";return false;}
    e.clear();return true;
}
namespace
{
struct Writer
{
    std::vector<uint8_t> b; bool ok=true;
    void U8(uint8_t v){if(b.size()>=64ull*1024*1024){ok=false;return;}b.push_back(v);}
    void U32(uint32_t v){for(int i=0;i<4;++i)U8(uint8_t(v>>(8*i)));}
    void U64(uint64_t v){for(int i=0;i<8;++i)U8(uint8_t(v>>(8*i)));}
    void I32(int32_t v){U32(uint32_t(v));}
    void Str(const std::string& s){if(s.size()>1024){ok=false;return;}U32(uint32_t(s.size()));for(unsigned char c:s)U8(c);}
    void Vec(I3 p){I32(p.x);I32(p.y);I32(p.z);}
    void Bounds(Box p){Vec(p.min);Vec(p.max);}
};
struct Reader
{
    const std::vector<uint8_t>& b;size_t p=0;bool ok=true;
    uint8_t U8(){if(p>=b.size()){ok=false;return 0;}return b[p++];}
    uint32_t U32(){uint32_t v=0;for(int i=0;i<4;++i)v|=uint32_t(U8())<<(8*i);return v;}
    uint64_t U64(){uint64_t v=0;for(int i=0;i<8;++i)v|=uint64_t(U8())<<(8*i);return v;}
    int32_t I32(){const uint32_t u=U32();return u<=uint32_t(INT32_MAX)?int32_t(u):int32_t(int64_t(u)-(1ll<<32));}
    uint32_t Count(uint32_t max){uint32_t n=U32();if(n>max){ok=false;return 0;}return n;}
    std::string Str(){uint32_t n=Count(1024);if(!ok||n>b.size()-p){ok=false;return {};}std::string s(b.begin()+p,b.begin()+p+n);p+=n;return s;}
    I3 Vec(){I3 v;v.x=I32();v.y=I32();v.z=I32();return v;}
    Box Bounds(){Box a;a.min=Vec();a.max=Vec();return a;}
};
void PutSettings(Writer& w,const Settings& s)
{
    const int32_t v[]={s.seed,s.cellCm,s.minZ,s.maxZ,s.sea,s.base,s.continentPeriod,s.erosionPeriod,s.mountainPeriod,s.climatePeriod,s.detailPeriod,s.continentAmplitude,s.mountainAmplitude,s.detailAmplitude,s.riverPeriod,s.riverWidthQ15,s.riverDepth,s.lakeSpacing,s.lakeRadius,s.lakeDepth,s.cavePeriod,s.chamberPeriod,s.caveWidthQ15,s.chamberThresholdQ15,s.aquiferSpacing,s.aquiferRadius,s.lavaCeiling,s.structureSpacing,s.structureChancePermille,s.maxSiteCutFill};
    for(int32_t x:v)w.I32(x);
}
}
bool EncodeCatalog(const Catalog& c,std::vector<uint8_t>& out,std::string& e)
{
    Writer w;w.U32(0x33434756);w.U32(CatalogFormat);w.U32(uint32_t(c.blocks.size()));for(const auto& s:c.blocks)w.Str(s);
    const uint16_t roles[]={c.palette.stone,c.palette.dirt,c.palette.grass,c.palette.sand,c.palette.snow,c.palette.water,c.palette.lava,c.palette.bedrock,c.palette.road};for(auto x:roles)w.U32(x);
    w.U32(uint32_t(c.trees.size()));for(const auto& t:c.trees){w.Str(t.key);w.U8(uint8_t(t.kind));w.U32(t.biomeMask);w.U32(t.trunk);w.U32(t.leaves);w.I32(t.spacing);w.I32(t.minHeight);w.I32(t.maxHeight);w.I32(t.trunkRadius);w.I32(t.crownRadius);w.I32(t.chancePermille);w.I32(t.maxSlope);}
    w.U32(uint32_t(c.plants.size()));for(const auto& p:c.plants){w.Str(p.key);w.U32(p.biomeMask);w.U32(p.block);w.I32(p.chancePermille);w.I32(p.patchPeriod);}
    w.U32(uint32_t(c.ores.size()));for(const auto& o:c.ores){w.Str(o.key);w.U32(o.block);w.I32(o.minZ);w.I32(o.maxZ);w.I32(o.spacing);w.I32(o.radius);w.I32(o.length);w.I32(o.chancePermille);w.U32(o.biomeMask);}
    w.U32(uint32_t(c.details.size()));for(const auto& d:c.details){w.Str(d.key);w.Str(d.assetPath);w.U64(d.geometryHash);w.Bounds(d.footprint);}
    w.U32(uint32_t(c.structures.size()));for(const auto& t:c.structures){w.Str(t.key);w.U8(uint8_t(t.kind));w.I32(t.sourceCellCm);w.I32(t.groundZ);w.Bounds(t.bounds);w.Vec(t.entrance);w.U32(uint32_t(t.writes.size()));for(const auto& r:t.writes){w.Vec(r.start);w.I32(r.length);w.U32(r.value);}w.U32(uint32_t(t.clear.size()));for(const auto& b:t.clear)w.Bounds(b);w.U32(uint32_t(t.details.size()));for(const auto& d:t.details){w.U32(d.asset);w.Vec(d.position);w.U8(d.yaw);}}
    if(!w.ok){e="Catalog exceeds canonical byte budget";return false;}out=std::move(w.b);e.clear();return true;
}
bool DecodeCatalog(const std::vector<uint8_t>& bytes,Catalog& out,std::string& e)
{
    if(bytes.size()>64ull*1024*1024){e="Catalog too large";return false;}Reader r{bytes};Catalog c;
    if(r.U32()!=0x33434756||r.U32()!=CatalogFormat){e="Catalog format mismatch";return false;}
    uint32_t n=r.Count(65535);for(uint32_t i=0;i<n&&r.ok;++i)c.blocks.push_back(r.Str());
    auto symbol=[&](){uint32_t v=r.U32();if(v>65535)r.ok=false;return uint16_t(v);};
    c.palette.stone=symbol();c.palette.dirt=symbol();c.palette.grass=symbol();c.palette.sand=symbol();c.palette.snow=symbol();c.palette.water=symbol();c.palette.lava=symbol();c.palette.bedrock=symbol();c.palette.road=symbol();
    n=r.Count(32);for(uint32_t i=0;i<n&&r.ok;++i){TreeRule t;t.key=r.Str();t.kind=TreeKind(r.U8());t.biomeMask=r.U32();t.trunk=symbol();t.leaves=symbol();t.spacing=r.I32();t.minHeight=r.I32();t.maxHeight=r.I32();t.trunkRadius=r.I32();t.crownRadius=r.I32();t.chancePermille=r.I32();t.maxSlope=r.I32();c.trees.push_back(t);}
    n=r.Count(64);for(uint32_t i=0;i<n&&r.ok;++i){PlantRule p;p.key=r.Str();p.biomeMask=r.U32();p.block=symbol();p.chancePermille=r.I32();p.patchPeriod=r.I32();c.plants.push_back(p);}
    n=r.Count(32);for(uint32_t i=0;i<n&&r.ok;++i){OreRule o;o.key=r.Str();o.block=symbol();o.minZ=r.I32();o.maxZ=r.I32();o.spacing=r.I32();o.radius=r.I32();o.length=r.I32();o.chancePermille=r.I32();o.biomeMask=r.U32();c.ores.push_back(o);}
    n=r.Count(4096);for(uint32_t i=0;i<n&&r.ok;++i){DetailDefinition d;d.key=r.Str();d.assetPath=r.Str();d.geometryHash=r.U64();d.footprint=r.Bounds();c.details.push_back(d);}
    n=r.Count(256);uint64_t total=0;
    for(uint32_t i=0;i<n&&r.ok;++i){StructureTemplate t;t.key=r.Str();t.kind=StructureKind(r.U8());t.sourceCellCm=r.I32();t.groundZ=r.I32();t.bounds=r.Bounds();t.entrance=r.Vec();uint32_t k=r.Count(1048576);total+=k;if(total>2097152){r.ok=false;break;}for(uint32_t j=0;j<k&&r.ok;++j){Run v;v.start=r.Vec();v.length=r.I32();v.value=r.U32();t.writes.push_back(v);}k=r.Count(4096);for(uint32_t j=0;j<k&&r.ok;++j)t.clear.push_back(r.Bounds());k=r.Count(4096);for(uint32_t j=0;j<k&&r.ok;++j){TemplateDetail d;d.asset=r.U32();d.position=r.Vec();d.yaw=r.U8();t.details.push_back(d);}c.structures.push_back(std::move(t));}
    if(!r.ok||r.p!=bytes.size()){e="Truncated, trailing or oversized catalog data";return false;}out=std::move(c);e.clear();return true;
}
uint64_t SettingsHash(const Settings& s,uint64_t h) {Writer w;w.U32(AlgorithmVersion);PutSettings(w,s);w.U64(h);return HashBytes(w.b);}
}
