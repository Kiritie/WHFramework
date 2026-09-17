#pragma once
#include "Voxel/Generation/Kernel/VoxelGenCatalog.h"
namespace VoxelGen
{
class WHFRAMEWORK_API Terrain
{
public:
    Terrain(const Settings& settings,const Palette& palette):s(settings),b(palette){}
    int32_t RawHeight(int32_t x,int32_t y) const;
    Column SampleColumn(int32_t x,int32_t y) const;
    Cell Geology(I3 p,const Column& column) const;
    bool SolidDensity(I3 p,int32_t height) const;
    bool Carved(I3 p,const Column& column) const;
private:
    Biome ResolveBiome(const Column& column) const;
    const Settings& s;
    const Palette& b;
};
}
