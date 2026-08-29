// Fill out your copyright notice in the Description page of Project Settings.

#include "Voxel/Generators/VoxelBuildingGenerator.h"

#include "Asset/AssetModuleStatics.h"
#include "Math/MathHelper.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleStatics.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Voxels/Data/VoxelData.h"

UVoxelBuildingGenerator::UVoxelBuildingGenerator()
{
	Seed = 673;
	SpawnRate = 0.001f;

	GenerateDatas = {
		FVoxelBuildingGenerateData(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Building_1")), 1.f),
		FVoxelBuildingGenerateData(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Building_2")), 1.f),
		FVoxelBuildingGenerateData(FPrimaryAssetId(TEXT("VoxelPrefab:DA_Building_3")), 1.f)
	};
}

void UVoxelBuildingGenerator::Initialize(UVoxelModule* InModule)
{
	Super::Initialize(InModule);

	_PrefabAssets.Reset();
	_PrefabAssets.Reserve(GenerateDatas.Num());
	for(const FVoxelBuildingGenerateData& GenerateData : GenerateDatas)
	{
		_PrefabAssets.Add(GenerateData.PrefabAsset.IsValid()
			? UAssetModuleStatics::LoadPrimaryAsset<UVoxelPrefabData>(GenerateData.PrefabAsset)
			: nullptr);
	}
}

void UVoxelBuildingGenerator::Generate(UVoxelChunk* InChunk)
{
	FScopeLock ScopeLock(&CriticalSection);

	if(!InChunk || !Module || GenerateDatas.IsEmpty() || _PrefabAssets.IsEmpty()) return;

	const FVector2D ChunkIndex = InChunk->GetIndex().ToVector2D();
	if(FMathHelper::HashRand(ChunkIndex, Seed) >= FMath::Clamp(SpawnRate, 0.f, 1.f)) return;

	const int32 BuildingIndex = SelectBuildingIndex(ChunkIndex + FVector2D(37.f, -71.f));
	if(BuildingIndex == INDEX_NONE) return;

	const FIndex ChunkWorldIndex = InChunk->GetWorldIndex();
	const FVector2D ChunkSize = Module->GetWorldData().ChunkSize;
	const int32 CenterX = ChunkWorldIndex.X + FMath::FloorToInt(ChunkSize.X * 0.5f);
	const int32 CenterY = ChunkWorldIndex.Y + FMath::FloorToInt(ChunkSize.Y * 0.5f);
	PlaceBuilding(CenterX, CenterY, BuildingIndex);
}

int32 UVoxelBuildingGenerator::SelectBuildingIndex(const FVector2D& InRandomPosition) const
{
	float TotalChance = 0.f;
	for(int32 i = 0; i < GenerateDatas.Num(); ++i)
	{
		if(_PrefabAssets.IsValidIndex(i) && IsValid(_PrefabAssets[i]))
		{
			TotalChance += FMath::Max(GenerateDatas[i].Chance, 0.f);
		}
	}

	if(TotalChance <= 0.f) return INDEX_NONE;

	const float RandomChance = FMathHelper::HashRand(InRandomPosition, Seed + 1) * TotalChance;
	float AccumulatedChance = 0.f;
	int32 LastValidIndex = INDEX_NONE;
	for(int32 i = 0; i < GenerateDatas.Num(); ++i)
	{
		if(!_PrefabAssets.IsValidIndex(i) || !IsValid(_PrefabAssets[i])) continue;

		const float Chance = FMath::Max(GenerateDatas[i].Chance, 0.f);
		if(Chance <= 0.f) continue;

		LastValidIndex = i;
		AccumulatedChance += Chance;
		if(RandomChance < AccumulatedChance)
		{
			return i;
		}
	}
	return LastValidIndex;
}

bool UVoxelBuildingGenerator::PlaceBuilding(int32 InX, int32 InY, int32 InBuildingIndex)
{
	if(!_PrefabAssets.IsValidIndex(InBuildingIndex)) return false;

	UVoxelPrefabData* PrefabAsset = _PrefabAssets[InBuildingIndex];
	if(!IsValid(PrefabAsset) || PrefabAsset->VoxelDatas.IsEmpty()) return false;

	TArray<FString> SerializedVoxelDatas;
	PrefabAsset->VoxelDatas.ParseIntoArray(SerializedVoxelDatas, TEXT("|"));

	TArray<FVoxelItem> VoxelItems;
	VoxelItems.Reserve(SerializedVoxelDatas.Num());
	int32 MinX = MAX_int32;
	int32 MaxX = MIN_int32;
	int32 MinY = MAX_int32;
	int32 MaxY = MIN_int32;
	int32 MaxZ = 0;
	for(const FString& SerializedVoxelData : SerializedVoxelDatas)
	{
		FVoxelItem VoxelItem(SerializedVoxelData, true);
		if(!VoxelItem.IsValid()) continue;

		VoxelItems.Add(VoxelItem);
		MinX = FMath::Min(MinX, VoxelItem.Index.X);
		MaxX = FMath::Max(MaxX, VoxelItem.Index.X);
		MinY = FMath::Min(MinY, VoxelItem.Index.Y);
		MaxY = FMath::Max(MaxY, VoxelItem.Index.Y);
		MaxZ = FMath::Max(MaxZ, VoxelItem.Index.Z);
	}

	if(VoxelItems.IsEmpty()) return false;

	TArray<FIndex> EntranceIndices;
	switch(GenerateDatas[InBuildingIndex].FrontDirection)
	{
		case ERightAngle::RA_0:
		{
			for(int32 Y = MinY; Y <= MaxY; ++Y) EntranceIndices.Emplace(MaxX, Y, 0);
			break;
		}
		case ERightAngle::RA_90:
		{
			for(int32 X = MinX; X <= MaxX; ++X) EntranceIndices.Emplace(X, MaxY, 0);
			break;
		}
		case ERightAngle::RA_180:
		{
			for(int32 Y = MinY; Y <= MaxY; ++Y) EntranceIndices.Emplace(MinX, Y, 0);
			break;
		}
		case ERightAngle::RA_270:
		default:
		{
			for(int32 X = MinX; X <= MaxX; ++X) EntranceIndices.Emplace(X, MinY, 0);
			break;
		}
	}

	const int32 RandomStartRotation = FMathHelper::HashRandInt(FVector2D(InX, InY), Seed + 2) % 4;
	int32 SelectedRotation = INDEX_NONE;
	int32 SelectedGroundHeight = 0;
	int32 SelectedMinX = 0;
	int32 SelectedMaxX = 0;
	int32 SelectedMinY = 0;
	int32 SelectedMaxY = 0;
	int32 BestEntranceMaxDrop = MAX_int32;
	int64 BestEntranceTotalDrop = MAX_int64;

	for(int32 RotationOffset = 0; RotationOffset < 4; ++RotationOffset)
	{
		const int32 Rotation = (RandomStartRotation + RotationOffset) % 4;
		const ERightAngle RotationAngle = static_cast<ERightAngle>(Rotation);
		const FIndex RotationIndexOffset = UVoxelModuleStatics::RightAngleToVoxelIndex(RotationAngle);

		int32 RotatedMinX = MAX_int32;
		int32 RotatedMaxX = MIN_int32;
		int32 RotatedMinY = MAX_int32;
		int32 RotatedMaxY = MIN_int32;
		for(const FVoxelItem& VoxelItem : VoxelItems)
		{
			const FIndex RotatedIndex = FMathHelper::RotateIndex(VoxelItem.Index, RotationAngle) + RotationIndexOffset;
			RotatedMinX = FMath::Min(RotatedMinX, RotatedIndex.X);
			RotatedMaxX = FMath::Max(RotatedMaxX, RotatedIndex.X);
			RotatedMinY = FMath::Min(RotatedMinY, RotatedIndex.Y);
			RotatedMaxY = FMath::Max(RotatedMaxY, RotatedIndex.Y);
		}

		double AverageHeight = 0.0;
		int32 FootprintCount = 0;
		bool bCanPlace = true;
		for(int32 X = RotatedMinX; X <= RotatedMaxX && bCanPlace; ++X)
		{
			for(int32 Y = RotatedMinY; Y <= RotatedMaxY; ++Y)
			{
				const FIndex SurfaceIndex(InX + X, InY + Y, Module->GetTopographyByIndex(FIndex(InX + X, InY + Y)).Height);
				if(!Module->HasVoxelByIndex(SurfaceIndex, true))
				{
					bCanPlace = false;
					break;
				}

				AverageHeight += SurfaceIndex.Z;
				++FootprintCount;
			}
		}
		if(!bCanPlace || FootprintCount <= 0) continue;

		const int32 GroundHeight = FMath::RoundToInt(AverageHeight / FootprintCount);
		if(GroundHeight <= Module->GetWorldData().SeaLevel) continue;

		int32 EntranceMaxDrop = 0;
		int64 EntranceTotalDrop = 0;
		for(const FIndex& EntranceIndex : EntranceIndices)
		{
			const FIndex RotatedEntranceIndex = FMathHelper::RotateIndex(EntranceIndex, RotationAngle) + RotationIndexOffset;
			const int32 EntranceHeight = Module->GetTopographyByIndex(FIndex(InX + RotatedEntranceIndex.X, InY + RotatedEntranceIndex.Y)).Height;
			const int32 EntranceDrop = FMath::Max(GroundHeight - EntranceHeight, 0);
			EntranceMaxDrop = FMath::Max(EntranceMaxDrop, EntranceDrop);
			EntranceTotalDrop += EntranceDrop;
		}

		if(EntranceMaxDrop < BestEntranceMaxDrop ||
			(EntranceMaxDrop == BestEntranceMaxDrop && EntranceTotalDrop < BestEntranceTotalDrop))
		{
			SelectedRotation = Rotation;
			SelectedGroundHeight = GroundHeight;
			SelectedMinX = RotatedMinX;
			SelectedMaxX = RotatedMaxX;
			SelectedMinY = RotatedMinY;
			SelectedMaxY = RotatedMaxY;
			BestEntranceMaxDrop = EntranceMaxDrop;
			BestEntranceTotalDrop = EntranceTotalDrop;
		}
	}

	if(SelectedRotation == INDEX_NONE) return false;

	for(int32 X = SelectedMinX; X <= SelectedMaxX; ++X)
	{
		for(int32 Y = SelectedMinY; Y <= SelectedMaxY; ++Y)
		{
			const int32 SurfaceHeight = Module->GetTopographyByIndex(FIndex(InX + X, InY + Y)).Height;
			for(int32 Z = SurfaceHeight; Z <= SelectedGroundHeight; ++Z)
			{
				Module->SetVoxelByIndex(FIndex(InX + X, InY + Y, Z), EVoxelType::Cobble_Stone);
			}
		}
	}

	const int32 ClearHeight = FMath::Max(FMath::CeilToInt(PrefabAsset->VoxelSize.Z), MaxZ);
	for(int32 X = SelectedMinX - 1; X <= SelectedMaxX + 1; ++X)
	{
		for(int32 Y = SelectedMinY - 1; Y <= SelectedMaxY + 1; ++Y)
		{
			for(int32 Z = 1; Z <= ClearHeight; ++Z)
			{
				Module->SetVoxelByIndex(FIndex(InX + X, InY + Y, SelectedGroundHeight + Z), FVoxelItem::Empty, true);
			}
		}
	}

	const ERightAngle SelectedRotationAngle = static_cast<ERightAngle>(SelectedRotation);
	const FIndex SelectedRotationIndexOffset = UVoxelModuleStatics::RightAngleToVoxelIndex(SelectedRotationAngle);
	const FIndex BuildingOrigin(InX, InY, SelectedGroundHeight);
	for(const FVoxelItem& VoxelItem : VoxelItems)
	{
		FVoxelItem RotatedVoxelItem = VoxelItem;
		if(RotatedVoxelItem.GetData().bRotatable)
		{
			RotatedVoxelItem.Angle = FMathHelper::CombineRightAngle(RotatedVoxelItem.Angle, SelectedRotationAngle);
		}
		const FIndex RotatedIndex = FMathHelper::RotateIndex(RotatedVoxelItem.Index, SelectedRotationAngle) + SelectedRotationIndexOffset;
		Module->SetVoxelByIndex(BuildingOrigin + RotatedIndex, RotatedVoxelItem);
	}
	return true;
}
