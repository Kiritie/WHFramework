#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct FVoxelStreamingSource;
struct FVoxelViewSettings;
struct FVoxelSectionMeshResult;

struct WHFRAMEWORK_API FVoxelFinePageKey
{
	FIntVector Coordinate = FIntVector::ZeroValue;
	uint8 Level = 0;
	static constexpr int32 BaseSide = 16;

	int32 GetSide() const { return BaseSide << Level; }
	FVoxelGenerationBounds GetBounds() const
	{
		const FIntVector Min = Coordinate * GetSide();
		return {Min, Min + FIntVector(GetSide())};
	}
	bool operator==(const FVoxelFinePageKey& InOther) const = default;
};

FORCEINLINE uint32 GetTypeHash(const FVoxelFinePageKey& InKey)
{
	return HashCombineFast(GetTypeHash(InKey.Coordinate), GetTypeHash(InKey.Level));
}

struct WHFRAMEWORK_API FVoxelFinePagePlan
{
	TSet<FVoxelFinePageKey> Pages;
	TMap<FIntVector, FVoxelFinePageKey> SectionOwners;
};

class WHFRAMEWORK_API FVoxelFinePagePlanner
{
public:
	static uint8 ResolveDesiredLevel(const FIntVector& InSection,
		TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelViewSettings& InSettings);
	static bool Build(const TSet<FIntVector>& InSections, TConstArrayView<FVoxelStreamingSource> InSources,
		const FVoxelViewSettings& InSettings, FVoxelFinePagePlan& OutPlan, FString& OutError);
	static void GatherSections(const FVoxelFinePageKey& InPage, TArray<FIntVector>& OutSections);
	static void AppendMesh(const FVoxelSectionMeshResult& InMesh, const FVector& InOffsetCells,
		FVoxelSectionMeshResult& OutMesh);

private:
	static uint8 ResolveBoundsLevel(const FVoxelGenerationBounds& InBounds,
		TConstArrayView<FVoxelStreamingSource> InSources, const FVoxelViewSettings& InSettings);
};
