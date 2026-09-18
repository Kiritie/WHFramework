#include "Voxel/Chunks/VoxelSectionSnapshot.h"
#include "Voxel/Chunks/VoxelSectionKey.h"
bool FVoxelSectionSnapshot::TrySample(const FIntVector&L,FVoxelBlockState&O)const
{
    if(L.X>=0&&L.X<16&&L.Y>=0&&L.Y<16&&L.Z>=0&&L.Z<16)
    {if(Blocks.Num()!=4096)return false;O=FVoxelBlockState::Unpack(Blocks[VoxelCoord::Linear(L)]);return true;}
    int32 Axis=-1,F=-1;
    for(int32 A=0;A<3;++A)if(L[A]<0||L[A]>=16){if(Axis!=-1)return false;Axis=A;F=A*2+(L[A]<0?1:0);}
    if(F<0||!Known[F]||Halo[F].Num()!=256)return false;
    int32 U=(Axis+1)%3,V=(Axis+2)%3;if(L[U]<0||L[U]>=16||L[V]<0||L[V]>=16)return false;
    O=FVoxelBlockState::Unpack(Halo[F][L[U]+16*L[V]]);return true;
}
uint64 FVoxelSectionSnapshot::Bytes()const
{uint64 N=uint64(Blocks.Num())*4;for(const auto&H:Halo)N+=uint64(H.Num())*4;return N;}
