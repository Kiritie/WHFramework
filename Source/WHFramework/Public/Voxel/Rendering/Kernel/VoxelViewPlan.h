#pragma once
#include "Voxel/Generation/Kernel/VoxelGenQuery.h"
#include <functional>
#include <set>
namespace VoxelView
{
using VoxelGen::I3;using VoxelGen::Cell;using VoxelGen::Box;
struct WHFRAMEWORK_API Key
{
    I3 coordinate;
    uint8_t level=0;
    bool operator==(const Key& b) const {return level==b.level&&coordinate==b.coordinate;}
    bool operator<(const Key& b) const {return std::tie(level,coordinate)<std::tie(b.level,b.coordinate);}
    int32_t Step() const {return 1<<level;}
    int32_t Side() const {return 16<<level;}
    Box Bounds() const;
    Key Parent() const;
    std::array<Key,8> Children() const;
};
struct WHFRAMEWORK_API Settings
{
    int32_t nearRadiusCm=3200,farRadiusCm=51200,verticalRadiusCm=25600;
    uint8_t maxLevel=4;
    uint32_t maxLeaves=16384;
    bool Validate() const;
};
struct WHFRAMEWORK_API Grid
{
    Key key;
    std::array<Cell,4096> cells{};
    uint64_t version=0,hash=0;
    bool natural=true;
    Cell Sample(I3 world) const;
};
struct Override {I3 position;Cell value=0;bool operator<(const Override& b) const{return position<b.position;}};
WHFRAMEWORK_API bool Select(const Settings& view,const VoxelGen::Settings& world,I3 observer,std::vector<Key>& leaves,std::string& error);
WHFRAMEWORK_API bool SelectMany(const Settings& view,const VoxelGen::Settings& world,const std::vector<I3>& observers,const std::set<Key>& forcedSplit,std::vector<Key>& leaves,std::string& error);
WHFRAMEWORK_API bool IsBalanced(const std::vector<Key>& leaves);
WHFRAMEWORK_API bool BuildProxy(const VoxelGen::Settings& settings,std::shared_ptr<const VoxelGen::Catalog> catalog,Key key,const std::vector<Override>& completeOverrides,Grid& out,std::string& error,const std::atomic_bool* cancel=nullptr);
struct Ticket {uint64_t epoch=0,serial=0;Key key;};
struct Node
{
    Ticket wanted;
    uint64_t accepted=0;
    bool dirty=true,pending=false,ready=false;
};
class WHFRAMEWORK_API Versions
{
public:
    explicit Versions(uint64_t epoch):epoch(epoch){}
    Ticket Request(Key key);
    void Invalidate(Box cells);
    bool Accept(Ticket ticket);
    void Fail(Ticket ticket);
    void Release(Key key);
    const Node* Find(Key key) const;
private:
    uint64_t epoch=0,nextSerial=1;
    std::map<Key,Node> nodes;
};
}
