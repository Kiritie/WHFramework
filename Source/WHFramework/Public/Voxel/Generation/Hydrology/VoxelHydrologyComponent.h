#pragma once

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyMacroInput.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyLocalRunoff.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyChannelPlan.h"

struct FVoxelGenerationRecipe;
struct FVoxelGenerationRuntimeConfig;
class FVoxelGenerationPlanCache;
class FVoxelTerrainGenerator;

enum class EVoxelHydrologyComponentStep : uint8
{
    None,
    NeedTiles,
    Yielded,
    CapacityBlocked,
    Ready,
    Conflict,
    Failed,
    Canceled
};

struct WHFRAMEWORK_API FVoxelHydrologyRiverComponent
{
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
    FIntPoint Key = FIntPoint::ZeroValue;
    uint64 Fingerprint = 0;
    uint64 TotalRainWeight = 0;
    uint64 IncomingRainWeight = 0;
    uint64 ReceivedRainWeight = 0;
    uint64 ExportedRainWeight = 0;
    TArray<FIntPoint> DrainageCells;
    FVoxelHydrologyPlan Plan;

    uint64 GetAllocatedBytes() const;
    bool ContainsCell(const FIntPoint& InCell) const;
    bool HasEquivalentOutput(const FVoxelHydrologyRiverComponent& InOther) const;
    bool Sample(int32 InWorldX, int32 InWorldY, int32 InOriginalGround,
        FVoxelHydrologyInfluence& OutInfluence) const;
};

struct WHFRAMEWORK_API FVoxelHydrologyComponentNode
{
    FIntPoint Cell = FIntPoint::ZeroValue;
    FIntPoint ParentCell = FIntPoint::ZeroValue;
    FVoxelRiverRoutePoint RoutePoint;
    int32 Parent = INDEX_NONE;
    int32 GroundZ = 0;
    uint64 RainWeight = 0;
    bool bChannelEligible = false;
    int32 DesiredWaterHalfWidth = 0;
    int32 DesiredDepth = 0;
    int32 ReceiverWaterZ = MIN_int32;
    FVoxelStableId ReceiverId;
    bool bHasParent = false;
    bool bSelected = false;
    bool bReceiver = false;
    bool bGhost = false;
};

class WHFRAMEWORK_API FVoxelHydrologyComponentBuild
{
public:
    using FTileLookup = FVoxelHydrologyMacroInput::FTileLookup;

    FVoxelHydrologyComponentBuild(
        TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
        FVoxelGenerationPlanCache& InCache, const FIntPoint& InSeedCell);
    ~FVoxelHydrologyComponentBuild();

    EVoxelHydrologyComponentStep Advance(FTileLookup InLookup, int32 InWorkBudget,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);
    const TArray<FVoxelHydrologyRegionKey>& GetMissingTiles() const;
    uint64 GetAllocatedBytes() const;
    const FVoxelHydrologyRiverComponent& GetResult() const;
    FVoxelHydrologyRiverComponent TakeResult();

    static bool CollectCoverageSeeds(const FVoxelHydrologyRegionKey& InCoverage,
        const FVoxelGenerationSettings& InSettings, FTileLookup InLookup,
        TArray<FIntPoint>& OutSeeds, TArray<FVoxelHydrologyRegionKey>& OutMissingTiles,
        FString& OutError);

    void VisitSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const;

private:
    friend class FVoxelGenerationPlanCache;
    bool PrepareRiverNodes(FTileLookup InLookup, FString& OutError, const TAtomic<bool>* InCancel);
    bool ReadExteriorNode(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologyComponentNode& OutNode, FString& OutError, const TAtomic<bool>* InCancel);
    void SetShape(FVoxelHydrologyComponentNode& InOutNode) const;
    bool BuildGeometry(FString& OutError, const TAtomic<bool>* InCancel);
    bool FinalizeProfileResult(FString& OutError, const TAtomic<bool>* InCancel);
    bool AddMissingTile(const FVoxelHydrologyRegionKey& InKey, FString& OutError);

private:
    TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config;
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
    FVoxelGenerationPlanCache& Cache;
    FIntPoint SeedCell = FIntPoint::ZeroValue;
    FVoxelHydrologyRegionKey Region;
    uint64 Generation = 0;
    TSharedPtr<const FVoxelHydrologyRegionGraph, ESPMode::ThreadSafe> Graph;
    TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Runoff;
    TSharedPtr<const FVoxelHydrologySourcePlan, ESPMode::ThreadSafe> Sources;
    TSharedPtr<const FVoxelHydrologyChannelPlan, ESPMode::ThreadSafe> Channels;
    TOptional<FVoxelHydrologyLocalRunoffBuild> LocalBuild;
    FVoxelHydrologyLocalRunoff Local;
    bool bLocalReady = false;
    bool bCertifiedGeometryInitialized = false, bCertifiedGeometryRouteStarted = false;
    int32 CertifiedGeometryNode = 0, CertifiedGeometryRiver = INDEX_NONE, CertifiedGeometryPoint = 0;
    int32 MacroGeometryIndex = 0;
    int32 CertifiedFinalStage = 0, CertifiedIndexRiver = 0, CertifiedIndexPoint = 0;
    int32 CertifiedFallStart = 0, CertifiedFallEnd = 1, CertifiedIndexCapacity = 0, CertifiedHashCursor = 0;
    FIntPoint CertifiedIndexMin, CertifiedIndexMax, CertifiedIndexCell;
    bool CertifiedIndexBucketActive = false;
    uint64 CertifiedHash = 0;
    TArray<FVoxelHydrologyComponentNode> Nodes;
    TArray<FVoxelHydrologyRegionKey> MissingTiles;
    TArray<int32> PrepareChildren;
    TArray<int32> PrepareQueue;
    TArray<int32> PrepareRemap;
    TArray<uint8> PrepareActive;
    TArray<FVoxelHydrologyComponentNode> PrepareSelected;
    TMap<FIntPoint, int32> ExteriorNodes;
    int32 PrepareStage = 0;
    int32 PrepareCursor = 0;
    int32 PrepareSourceCursor = 0;
    int32 PrepareActiveCount = 0;
    bool bProfilesSolved = false;
    bool bNodesFiltered = false;
    bool bGeometryBuilt = false;
    bool bReady = false;
    FVoxelHydrologyRiverComponent Result;
};
