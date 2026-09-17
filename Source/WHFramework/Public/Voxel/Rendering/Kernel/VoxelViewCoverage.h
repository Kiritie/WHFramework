#pragma once
#include "Voxel/Rendering/Kernel/VoxelViewPlan.h"
#include <set>
namespace VoxelView
{
struct Change
{
    uint64_t serial=0,generation=0;
    std::vector<Key> remove,add,remesh;
};
// Keys and visibility topology only. Grids, meshes, persistence and threads are owned elsewhere.
class WHFRAMEWORK_API Coverage
{
public:
    explicit Coverage(uint8_t maxLevel):maxLevel(maxLevel){}
    void BlockParent(Key key){blocked.insert(key);}
    bool SetDesired(const std::vector<Key>& leaves);
    bool Propose(const std::function<bool(Key)>& gridReady,std::vector<Key>& request,Change& out);
    bool Replacement(Key key,Change& out);
    bool Commit(const Change& change);
    const std::set<Key>& Visible() const {return active;}
    bool Wants(Key key) const;
    bool FindOwner(I3 point,Key& out) const;
    uint64_t Generation() const {return generation;}
    void Clear();
private:
    bool Balanced(const std::set<Key>& keys) const;
    bool Build(std::vector<Key> remove,std::vector<Key> add,Change& out);
    std::set<Key> Apply(const Change& c) const;
    uint8_t maxLevel=4;
    uint64_t nextSerial=1,generation=1;
    std::set<Key> target,nodes,roots,active,blocked;
};
}
