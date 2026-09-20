#pragma once

#include "CoreMinimal.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

class FVoxelTerrainGenerator;
class FVoxelHydrologyGenerator;

struct WHFRAMEWORK_API FVoxelCaveAnchor
{
    FVoxelStableId Id;

    FIntVector Entrance = FIntVector::ZeroValue;
    FIntVector Target = FIntVector::ZeroValue;

    int32 Radius = 0;
};

struct WHFRAMEWORK_API FVoxelCaveRouteSection
{
	FIntVector Center = FIntVector::ZeroValue;
	int32 Radius = 0;
	int32 FloorZ = 0;
	int32 CeilingZ = 0;
};

struct WHFRAMEWORK_API FVoxelCaveRoute
{
    FVoxelStableId Id;
	TArray<FVoxelCaveRouteSection> Sections;

	bool Carves(const FIntVector& InCell) const;
	bool ProtectsFloor(const FIntVector& InCell) const;
};

struct WHFRAMEWORK_API FVoxelCaveChamber
{
    FVoxelStableId Id;

    FIntVector Center = FIntVector::ZeroValue;
    FIntVector Radius = FIntVector::ZeroValue;
};

struct WHFRAMEWORK_API FVoxelCavePlan
{
    TArray<FVoxelCaveRoute> Routes;
    TArray<FVoxelCaveChamber> Chambers;

    bool Carves(const FIntVector& InCell) const;
	bool ProtectsFloor(const FIntVector& InCell) const;
    uint64 GetAllocatedBytes() const;
};

class WHFRAMEWORK_API FVoxelCaveGenerator
{
public:
    FVoxelCaveGenerator(
        TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
        TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
        TSharedRef<const FVoxelHydrologyGenerator, ESPMode::ThreadSafe> InHydrology);

public:
    bool BuildPlan(
        const FVoxelGenerationBounds& InBounds,
        FVoxelCavePlan& OutPlan,
		FString& OutError,
        const TAtomic<bool>* InCancel = nullptr) const;

private:
    void GatherAnchors(
        const FVoxelGenerationBounds& InBounds,
        TArray<FVoxelCaveAnchor>& OutAnchors) const;

    bool BuildMainRoute(
        const FVoxelCaveAnchor& InAnchor,
		FVoxelCaveRoute& OutRoute,
		FString& OutError) const;

    void AddBranches(
        const FVoxelCaveAnchor& InAnchor,
        const FVoxelCaveRoute& InMainRoute,
        TArray<FVoxelCaveRoute>& OutRoutes) const;

	bool ValidateWalkRoute(const FVoxelCaveRoute& InRoute, const FIntVector& InEntrance,
		const FIntVector& InTarget, FString& OutError) const;

    void AddChambers(
        const FVoxelCaveAnchor& InAnchor,
        const FVoxelCaveRoute& InMainRoute,
        TArray<FVoxelCaveChamber>& OutChambers) const;

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
    TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
    TSharedRef<const FVoxelHydrologyGenerator, ESPMode::ThreadSafe> Hydrology;
};
