#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Engine/Texture2DArray.h"

namespace
{
int32 GetTextureArraySliceCount(const UTexture2DArray& Texture)
{
#if WITH_EDITORONLY_DATA
	if (Texture.Source.IsValid())
	{
		return Texture.Source.GetNumSlices();
	}
#endif

	return Texture.GetArraySize();
}
}

const FVoxelMaterialBank* UVoxelMaterialSet::FindBank(EVoxelRenderGroup G, uint16 B) const
{
	for (const auto& V : Banks)
		if (V.Group == G && V.Bank == B)
			return &V;
	return nullptr;
}
bool UVoxelMaterialSet::Validate(FString& E) const
{
	if (BakeVersion != 2)
	{
		E = TEXT("MaterialSet bake version mismatch");
		return false;
	}
	TSet<uint32> Seen;
	for (const auto& B : Banks)
	{
		if (B.Group == EVoxelRenderGroup::None || B.Group > EVoxelRenderGroup::Emissive || B.Bank < 0 || B.Bank > 65535 || B.SliceCount <= 0 ||
		    B.SliceCount > 65536 || !B.Textures || !B.Material)
		{
			E = TEXT("Invalid material bank");
			return false;
		}
		uint32 K = (uint32(B.Group) << 16) | uint32(B.Bank);
		if (Seen.Contains(K))
		{
			E = TEXT("Duplicate material bank");
			return false;
		}
		Seen.Add(K);
		if (GetTextureArraySliceCount(*B.Textures) != B.SliceCount)
		{
			E = TEXT("Texture array slice count mismatch");
			return false;
		}
	}
	E.Reset();
	return true;
}
