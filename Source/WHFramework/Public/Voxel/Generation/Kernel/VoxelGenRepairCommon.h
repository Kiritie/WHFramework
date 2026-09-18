#pragma once
// Value-only helpers owned by the existing UVoxelModule collaborators.
#if !defined(VOXEL_REPAIR_STANDALONE)
#include "CoreMinimal.h"
#else
#ifndef WHFRAMEWORK_API
#define WHFRAMEWORK_API
#endif
#endif
#include <algorithm>
#include <atomic>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>
namespace VoxelRepair  {
    struct Point  {
        int32_t x=0,y=0,z=0;
        bool operator==(const Point& b) const  {
            return x==b.x&&y==b.y&&z==b.z;
        }
        bool operator<(const Point& b) const  {
            return std::tie(z,y,x)<std::tie(b.z,b.y,b.x);
        }
    };
    using Cell=uint32_t;
    inline bool Canceled(const std::atomic_bool* c) {
        return c&&c->load(std::memory_order_relaxed);
    }
    inline bool ValidPoint(Point p)  {
        constexpr int64_t m=int64_t {
            1
        }
        <<24;
        return int64_t(p.x)>=-m&&int64_t(p.x)<m&&int64_t(p.y)>=-m&&int64_t(p.y)<m &&p.z>=-32768&&p.z<32768;
    }
    inline uint64_t Mix(uint64_t v)  {
        v+=0x9e3779b97f4a7c15ULL;
        v=(v^(v>>30))*0xbf58476d1ce4e5b9ULL;
        v=(v^(v>>27))*0x94d049bb133111ebULL;
        return v^(v>>31);
    }
    inline int32_t FloorDiv(int32_t v,int32_t positiveDivisor)  {
        // Caller validation must establish a positive divisor.
        const int32_t q=v/positiveDivisor,r=v%positiveDivisor;
        return q-(r<0);
    }
    inline int32_t SmoothQ16(int32_t t)  {
        const int64_t q=std::clamp(t,0,65536);
        return int32_t(q*q*(196608-2*q)/(65536LL*65536LL));
    }
}
