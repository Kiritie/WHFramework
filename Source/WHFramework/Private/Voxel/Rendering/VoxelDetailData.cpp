#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
UVoxelDetailData::UVoxelDetailData(){Type=TEXT("VoxelDetail");}
bool UVoxelDetailData::Validate(FString& E) const
{
    if(StableKey.IsNone()||Mesh.IsNull()||BakeVersion!=1||!GeometryHash){E=TEXT("Detail requires a stable key and baked mesh fingerprint");return false;}
    if(FootprintCells.Min.X>=FootprintCells.Max.X||FootprintCells.Min.Y>=FootprintCells.Max.Y||FootprintCells.Min.Z>=FootprintCells.Max.Z||CollisionBoxesCm.Num()>32){E=TEXT("Invalid detail footprint/collision budget");return false;}
    for(const FBox& B:CollisionBoxesCm)if(!B.IsValid||B.Min.ContainsNaN()||B.Max.ContainsNaN()||B.GetSize().GetMin()<=0){E=TEXT("Invalid detail collision box");return false;}
    if(bNearCollision&&CollisionBoxesCm.IsEmpty()){E=TEXT("Near-collision detail needs explicit boxes");return false;}
    if(!MetadataHash || MetadataHash!=ComputeMetadataHash()){E=TEXT("Detail metadata changed after bake");return false;}
    E.Reset();return true;
}

uint64 UVoxelDetailData::ComputeMetadataHash() const
{
    FVoxelByteWriter W(8192);
    W.String(StableKey.ToString()); W.String(Mesh.ToSoftObjectPath().ToString()); W.U8(bNearCollision);
    for(int32 A=0;A<3;++A){W.I32(FootprintCells.Min[A]);W.I32(FootprintCells.Max[A]);}
    W.U32(CollisionBoxesCm.Num());
    for(const FBox& B:CollisionBoxesCm)for(int32 A=0;A<3;++A)
    {
        const double Lo=B.Min[A]*100.0,Hi=B.Max[A]*100.0;
        if(!FMath::IsFinite(Lo)||!FMath::IsFinite(Hi)||FMath::Abs(Lo)>2e9||FMath::Abs(Hi)>2e9)return 0;
        W.I32(FMath::RoundToInt(Lo));W.I32(FMath::RoundToInt(Hi));
    }
    TArray<uint8> Bytes;return W.Finish(Bytes)?VoxelBinary::Hash(Bytes):0;
}
