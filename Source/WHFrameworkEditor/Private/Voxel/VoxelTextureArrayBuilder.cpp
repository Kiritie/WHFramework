#include "Voxel/VoxelTextureArrayBuilder.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Voxel/VoxelMaterialBuilder.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "ImageCore.h"
#include "Misc/PackageName.h"
#include "TextureCompiler.h"
namespace
{
struct FSource
{
	FString Key;
	FVoxelFaceTexture Face;
	EVoxelRenderGroup Group;
	TArray<uint8> Pixels;
	FVoxelBakedFaceRef Baked;
};
FString KeyOf(EVoxelRenderGroup Group, const FVoxelFaceTexture& F)
{
	return FString::Printf(TEXT("%u|%s|%d|%d"), uint8(Group), *F.Texture.ToSoftObjectPath().ToString(), F.FrameCount, F.FramesPerSecond);
}
bool ReadFrames(FSource& S, int32 Tile, FString& Error)
{
	UTexture2D* T = S.Face.Texture.LoadSynchronous();
	if (!T || !T->Source.IsValid() || S.Face.FrameCount < 1 || S.Face.FrameCount > 256 || S.Face.FramesPerSecond < 0 || S.Face.FramesPerSecond > 60)
	{
		Error = TEXT("Invalid source texture: ") + S.Key;
		return false;
	}
	const int64 SourceX = T->Source.GetSizeX(), SourceY = T->Source.GetSizeY();
	if (SourceX <= 0 || SourceX > 4096 || SourceY <= 0 || SourceY > 1048576 || SourceX * SourceY * 4 > 64ll * 1024 * 1024)
	{
		Error = TEXT("Source texture decoded pixels exceed 64 MiB: ") + S.Key;
		return false;
	}
	FImage Image, BGRA;
	if (!T->Source.GetMipImage(Image, 0) || Image.NumSlices != 1 || Image.SizeX <= 0 || Image.SizeX > 4096 ||
	    Image.SizeY != int64(Image.SizeX) * S.Face.FrameCount)
	{
		Error = TEXT("Texture must be a square tile or declared vertical frame strip: ") + S.Key;
		return false;
	}
	Image.CopyTo(BGRA, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	S.Pixels.SetNumUninitialized(Tile * Tile * 4 * S.Face.FrameCount);
	const int32 W = BGRA.SizeX;
	for (int32 F = 0; F < S.Face.FrameCount; ++F)
		for (int32 Y = 0; Y < Tile; ++Y)
			for (int32 X = 0; X < Tile; ++X)
			{
				const int64 Src = (int64(F * W + (Y * W / Tile)) * W + X * W / Tile) * 4;
				const int64 Dst = (int64(F * Tile + Y) * Tile + X) * 4;
				FMemory::Memcpy(S.Pixels.GetData() + Dst, BGRA.RawData.GetData() + Src, 4);
			}
	return true;
}
}
bool FVoxelTextureArrayBuilder::Build(const TArray<UVoxelData*>& Assets, UVoxelMaterialSet& Set, const FVoxelArrayBuildOptions& O, FString& Error)
{
	check(IsInGameThread());
	Error.Reset();
	if (Assets.IsEmpty() || !FPackageName::IsValidLongPackageName(O.OutputRoot) || O.TileSize < 4 || O.TileSize > 256 || !FMath::IsPowerOfTwo(O.TileSize) ||
	    O.MaxSlicesPerBank < 1 || O.MaxSlicesPerBank > 2048)
	{
		Error = TEXT("Invalid array builder options");
		return false;
	}
	TArray<FSource> Sources;
	TMap<FString, int32> Lookup;
	TSet<FName> Names;
	for (UVoxelData* A : Assets)
	{
		if (!A || !A->ValidateDefinition(false, Error) || Names.Contains(A->BlockName))
		{
			if (Error.IsEmpty())
				Error = TEXT("Duplicate or invalid block asset");
			return false;
		}
		if (!A->bRegisterBlock)
		{
			continue;
		}
		Names.Add(A->BlockName);
		for (int32 H = 0; H < (A->Shape == EVoxelShapeKind::Door ? 2 : 1); ++H)
			for (uint8 F = 0; F < 6; ++F)
			{
				const auto& Face = (H ? A->UpperFaceMaterials : A->FaceMaterials).Get(F);
				if (Face.Texture.IsNull())
				{
					Error = FString::Printf(TEXT("Registered voxel %s has no source texture for half %d face %d"), *A->GetPathName(), H, F);
					return false;
				}
				const FString K = KeyOf(A->RenderGroup, Face);
				if (Face.FrameCount > O.MaxSlicesPerBank)
				{
					Error = TEXT("Animation exceeds bank slice limit: ") + K;
					return false;
				}
				if (!Lookup.Contains(K))
				{
					FSource S;
					S.Key = K;
					S.Face = Face;
					S.Group = A->RenderGroup;
					Lookup.Add(K, Sources.Add(MoveTemp(S)));
				}
			}
	}
	Sources.Sort(
	    [](const FSource& A, const FSource& B)
	    {
		    return A.Key < B.Key;
	    });
	Lookup.Reset();
	uint64 TotalNormalizedBytes = 0;
	for (int32 I = 0; I < Sources.Num(); ++I)
	{
		TotalNormalizedBytes += uint64(O.TileSize) * O.TileSize * 4 * Sources[I].Face.FrameCount;
		if (TotalNormalizedBytes > 512ull * 1024 * 1024)
		{
			Error = TEXT("Normalized texture source set exceeds 512 MiB; reduce tile/frame input");
			return false;
		}
		if (!ReadFrames(Sources[I], O.TileSize, Error))
			return false;
		Lookup.Add(Sources[I].Key, I);
	}
	// All pixel validation completes before published assets are modified.
	const FString BuildRoot = O.OutputRoot + TEXT("/Build_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	TArray<FVoxelMaterialBank> Banks;
	TMap<uint8, UMaterial*> Masters;
	int32 Cursor = 0;
	while (Cursor < Sources.Num())
	{
		const EVoxelRenderGroup G = Sources[Cursor].Group;
		uint32 Bank = 0;
		while (Cursor < Sources.Num() && Sources[Cursor].Group == G)
		{
			if (Bank > 65535)
			{
				Error = TEXT("Texture group exceeds 65536 banks");
				return false;
			}
			TArray<uint8> Pixels;
			int32 Count = 0;
			while (Cursor < Sources.Num() && Sources[Cursor].Group == G && Count + Sources[Cursor].Face.FrameCount <= O.MaxSlicesPerBank)
			{
				auto& S = Sources[Cursor++];
				S.Baked.Bank = Bank;
				S.Baked.Layer = Count;
				S.Baked.Frames = S.Face.FrameCount;
				S.Baked.FPS = S.Face.FramesPerSecond;
				Count += S.Face.FrameCount;
				Pixels.Append(S.Pixels);
			}
			const FString Stem = BuildRoot + FString::Printf(TEXT("/G%u_B%u"), uint8(G), Bank);
			auto* Array = Cast<UTexture2DArray>(FVoxelEditorAssetIO::LoadOrCreate(UTexture2DArray::StaticClass(), Stem + TEXT("_Array"), Error));
			if (!Array)
				return false;
			Array->SRGB = true;
			Array->CompressionSettings = TC_Default;
			Array->MipGenSettings = TMGS_FromTextureGroup;
			Array->Filter = TF_Nearest;
			Array->AddressX = TA_Wrap;
			Array->AddressY = TA_Wrap;
			Array->AddressZ = TA_Clamp;
			Array->Source.Init(O.TileSize, O.TileSize, Count, 1, TSF_BGRA8, Pixels.GetData());
			Array->PostEditChange();
			UTexture* TexturesToFinish[] = {Array};
			FTextureCompilingManager::Get().FinishCompilation(TexturesToFinish);
			if (Array->GetArraySize() != Count)
			{
				Error = FString::Printf(
					TEXT("Texture array %s contains %d slices after compilation; expected %d"),
					*Array->GetPathName(),
					Array->GetArraySize(),
					Count);
				return false;
			}
			if (!FVoxelEditorAssetIO::Save(Array, Error))
				return false;
			UMaterial*& Master = Masters.FindOrAdd(uint8(G));
			if (!Master)
				Master = FVoxelMaterialBuilder::BuildMaster(BuildRoot + FString::Printf(TEXT("/M_G%u"), uint8(G)), G, Array, Error);
			if (!Master)
				return false;
			auto* MI = FVoxelMaterialBuilder::BuildInstance(Stem + TEXT("_MI"), Master, Array, Error);
			if (!MI)
				return false;
			FVoxelMaterialBank B;
			B.Group = G;
			B.Bank = Bank++;
			B.SliceCount = Count;
			B.Textures = Array;
			B.Material = MI;
			Banks.Add(B);
		}
	}
	// Resource generation is complete. The following saves form a source-control batch, not an OS transaction.
	for (UVoxelData* A : Assets)
	{
		if (!A->bRegisterBlock)
		{
			continue;
		}
		A->Modify();
		A->BakedFaces.Reset();
		A->BakedUpperFaces.Reset();
		for (int32 H = 0; H < (A->Shape == EVoxelShapeKind::Door ? 2 : 1); ++H)
			for (uint8 F = 0; F < 6; ++F)
			{
				const auto& Face = (H ? A->UpperFaceMaterials : A->FaceMaterials).Get(F);
				(H ? A->BakedUpperFaces : A->BakedFaces).Add(Sources[Lookup.FindChecked(KeyOf(A->RenderGroup, Face))].Baked);
			}
		A->BakeVersion = 2;
	}
	Set.Modify();
	Set.Banks = MoveTemp(Banks);
	Set.BakeVersion = 2;
	for (UVoxelData* A : Assets)
		if (!FVoxelEditorAssetIO::Save(A, Error))
			return false;
	return FVoxelEditorAssetIO::Save(&Set, Error);
}
