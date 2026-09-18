#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Engine/Texture2DArray.h"
const FVoxelMaterialBank* UVoxelMaterialSet::FindBank(EVoxelRenderGroup G,uint16 B)const
{for(const auto&V:Banks)if(V.Group==G&&V.Bank==B)return &V;return nullptr;}
bool UVoxelMaterialSet::Validate(FString&E)const
{
    if(BakeVersion!=2){E=TEXT("MaterialSet bake version mismatch");return false;}TSet<uint32>Seen;
    for(const auto&B:Banks)
    {
        if(uint8(B.Group)>5||B.Bank<0||B.Bank>65535||B.SliceCount<=0||B.SliceCount>65536||!B.Textures||!B.Material)
        {E=TEXT("Invalid material bank");return false;}
        uint32 K=(uint32(B.Group)<<16)|uint32(B.Bank);if(Seen.Contains(K)){E=TEXT("Duplicate material bank");return false;}Seen.Add(K);
        int32 ArraySize=B.Textures->GetArraySize();
#if WITH_EDITORONLY_DATA
        if(B.Textures->Source.IsValid())
        {
            const int32 SourceSlices=B.Textures->Source.GetNumSlices();
            if(SourceSlices==B.SliceCount)
            {
                // In PIE the texture compiler may still expose stale platform
                // data (commonly SizeZ == 1) while the serialized source array
                // already contains every baked slice. The source is the
                // authoritative editor-time validation input.
                ArraySize=SourceSlices;
            }
        }
#endif
        if(ArraySize!=B.SliceCount)
        {
            E=FString::Printf(TEXT("Texture array slice count mismatch: group %d bank %d records %d, runtime array has %d"),
                int32(B.Group),B.Bank,B.SliceCount,ArraySize);
            return false;
        }
    }
    E.Reset();return true;
}
