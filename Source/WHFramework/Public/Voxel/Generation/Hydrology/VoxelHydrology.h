#pragma once

#include "CoreMinimal.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Templates/Atomic.h"
#include "Templates/SharedPointer.h"
#include "Voxel/Generation/VoxelGenerationTypes.h"

struct FVoxelGenerationRecipe;
class FVoxelTerrainGenerator;

struct WHFRAMEWORK_API FVoxelDrainageInput
{
    FIntPoint WorldMinCell = FIntPoint::ZeroValue;

    int32 Width = 0;
    int32 Height = 0;

    TArray<int32> GroundPlane;
    TArray<uint32> RainWeight;
    TArray<uint8> Allowed;

    /**
     * Cell index -> canonical receiving water plane.
     *
     * 只允许：
     * - Ocean Cell；
     * - Canonical Sink；
     * - 上层明确提供的跨域 Connector。
     *
     * Region 边缘不能自动当 Outlet。
     */
    TMap<uint32, int32> Outlets;
};

struct WHFRAMEWORK_API FVoxelDrainageResult
{
    TArray<int32> SpillPlane;
    TArray<int32> Parent;
    TArray<uint32> SettledOrder;
    TArray<uint64> Accumulation;
};

struct WHFRAMEWORK_API FVoxelBasinConnectorKey
{
    uint32 CellIndex = 0;
    uint8 Face = 0;

    bool operator==(const FVoxelBasinConnectorKey& InOther) const
    {
        return CellIndex == InOther.CellIndex &&
            Face == InOther.Face;
    }
};

FORCEINLINE uint32 GetTypeHash(const FVoxelBasinConnectorKey& InKey)
{
    return HashCombineFast(
        ::GetTypeHash(InKey.CellIndex),
        ::GetTypeHash(InKey.Face));
}

struct WHFRAMEWORK_API FVoxelBasinInput
{
    int32 Width = 0;
    int32 Height = 0;

    int32 WaterPlane = 0;

    TArray<int32> GroundPlane;
    TArray<uint8> Known;
    TArray<uint8> WaterMask;

    /**
     * 只有调用者已经验证过的外部水体连接，
     * 才能成为 Basin Connector。
     *
     * Plane 必须等于 WaterPlane。
     */
    TMap<FVoxelBasinConnectorKey, int32> ConnectorPlanes;
};

struct WHFRAMEWORK_API FVoxelBasinCertificate
{
    uint32 WaterCells = 0;
    uint32 ClosedEdges = 0;
    uint32 ConnectorEdges = 0;
};

struct WHFRAMEWORK_API FVoxelRiverShape
{
    int32 HalfWidth = 4;
    int32 BankWidth = 4;
    int32 ShoreWidth = 12;
    int32 Depth = 8;
    int32 MaxCutFill = 64;
};

struct WHFRAMEWORK_API FVoxelRiverSection
{
    int32 BedPlane = 0;
    int32 WaterPlane = 0;
    int32 GroundPlane = 0;
    bool bWet = false;
};

namespace VoxelHydrology
{
    WHFRAMEWORK_API bool BuildDrainage(
        const FVoxelDrainageInput& InInput,
        FVoxelDrainageResult& OutResult,
        FString& OutError,
        const TAtomic<bool>* InCancel = nullptr);

    WHFRAMEWORK_API bool TraceDrainage(
        const FVoxelDrainageInput& InInput,
        const FVoxelDrainageResult& InResult,
        uint32 InSource,
        int32 InRouteBudget,
        TArray<uint32>& OutPath,
        FString& OutError);

    WHFRAMEWORK_API bool ValidateBasin(
        const FVoxelBasinInput& InInput,
        FVoxelBasinCertificate& OutCertificate,
        FString& OutError,
        const TAtomic<bool>* InCancel = nullptr);

    WHFRAMEWORK_API bool EvaluateRiverSection(
        int32 InOriginalGround,
        int32 InWaterPlane,
        int32 InDistanceCells,
        const FVoxelRiverShape& InShape,
        FVoxelRiverSection& OutSection,
        FString& OutError);
}

struct WHFRAMEWORK_API FVoxelHydrologyGrid
{
    FIntPoint WorldMinCell = FIntPoint::ZeroValue;

    int32 Width = 0;
    int32 Height = 0;
    int32 CellSize = 1;

    TArray<int32> GroundPlane;
    TArray<uint32> RainWeight;
    TArray<uint8> Allowed;

    TMap<uint32, int32> OceanOutlets;
    TMap<uint32, int32> CanonicalSinkOutlets;

    bool IsValidIndex(uint32 InIndex) const;
    uint32 ToIndex(int32 InX, int32 InY) const;
    FIntPoint ToLocal(uint32 InIndex) const;
    FIntPoint ToWorldHydrologyCell(uint32 InIndex) const;
    FIntPoint ToWorldVoxelXY(uint32 InIndex) const;
};

struct WHFRAMEWORK_API FVoxelRiverSource
{
    FVoxelStableId Id;
    uint32 GridIndex = 0;
    uint64 Accumulation = 0;
};

struct WHFRAMEWORK_API FVoxelRiverRoutePoint
{
    FIntPoint Position = FIntPoint::ZeroValue;
    int32 WaterZ = 0;
    int32 HalfWidth = 0;
    int32 Depth = 0;
    uint64 Accumulation = 0;
};

struct WHFRAMEWORK_API FVoxelRiverRoute
{
    FVoxelStableId Id;
    FVoxelHydrologyRegionKey OwnerRegion;

    TArray<FVoxelRiverRoutePoint> Points;

    FIntPoint Min = FIntPoint::ZeroValue;
    FIntPoint Max = FIntPoint::ZeroValue;
};

struct WHFRAMEWORK_API FVoxelLakePlan
{
    FVoxelStableId Id;
    FVoxelHydrologyRegionKey OwnerRegion;

    int32 WaterZ = MIN_int32;

    TArray<FIntPoint> Cells;

    FVoxelBasinCertificate Certificate;
};

struct WHFRAMEWORK_API FVoxelRiverSegmentRef
{
	int32 RiverIndex = INDEX_NONE;
	int32 PointIndex = INDEX_NONE;
};

struct WHFRAMEWORK_API FVoxelHydrologyInfluence
{
    int32 SurfaceWaterZ = MIN_int32;
    int32 GroundOverrideZ = MIN_int32;

    bool bRiver = false;
    bool bLake = false;
    bool bOcean = false;
    bool bCoast = false;
};

struct WHFRAMEWORK_API FVoxelHydrologyPlan
{
    FVoxelHydrologyRegionKey Key;

	FIntPoint CoreMin =
		FIntPoint::ZeroValue;

	FIntPoint CoreMax =
		FIntPoint::ZeroValue;

    FVoxelHydrologyGrid Grid;
    FVoxelDrainageResult Drainage;

    TArray<FVoxelRiverRoute> Rivers;
    TArray<FVoxelLakePlan> Lakes;

	void Finalize();

    bool Sample(
        int32 InWorldX,
        int32 InWorldY,
        int32 InOriginalGround,
        FVoxelHydrologyInfluence& OutInfluence) const;

	uint64 GetAllocatedBytes() const;

private:
	TMap<
		FIntPoint,
		int32>
		LakeWaterByCellOrigin;

	TMap<
		FIntPoint,
		TArray<FVoxelRiverSegmentRef>>
		RiverSegmentsByHydrologyCell;
};

class WHFRAMEWORK_API FVoxelHydrologyGenerator
{
public:
    FVoxelHydrologyGenerator(
        TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
        TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain);

public:
    bool BuildPlan(
        const FVoxelHydrologyRegionKey& InKey,
        FVoxelHydrologyPlan& OutPlan,
        FString& OutError,
        const TAtomic<bool>* InCancel = nullptr) const;

private:
    bool BuildHeightGrid(
        const FVoxelHydrologyRegionKey& InKey,
        FVoxelHydrologyGrid& OutGrid,
        FString& OutError,
        const TAtomic<bool>* InCancel) const;

    void AddOceanOutlets(
        FVoxelHydrologyGrid& InOutGrid) const;

    void AddCanonicalSinkOutlets(
        FVoxelHydrologyGrid& InOutGrid) const;

    void SelectRiverSources(
        const FVoxelHydrologyGrid& InGrid,
        const FVoxelDrainageResult& InDrainage,
        const FVoxelHydrologyRegionKey& InOwnerRegion,
        TArray<FVoxelRiverSource>& OutSources) const;

    bool TraceRiver(
        const FVoxelHydrologyGrid& InGrid,
        const FVoxelDrainageInput& InInput,
        const FVoxelDrainageResult& InDrainage,
        const FVoxelHydrologyRegionKey& InOwnerRegion,
        const FVoxelRiverSource& InSource,
        TSet<uint32>& InOutClaimedDownstream,
        FVoxelRiverRoute& OutRoute,
        FString& OutError) const;

    bool BuildLakes(
        const FVoxelHydrologyGrid& InGrid,
        const FVoxelDrainageInput& InInput,
        const FVoxelDrainageResult& InDrainage,
        const FVoxelHydrologyRegionKey& InOwnerRegion,
        TArray<FVoxelLakePlan>& OutLakes,
        FString& OutError,
        const TAtomic<bool>* InCancel) const;

private:
    TSharedRef<
        const FVoxelGenerationRecipe,
        ESPMode::ThreadSafe> Recipe;

    TSharedRef<
        const FVoxelTerrainGenerator,
        ESPMode::ThreadSafe> Terrain;
};
