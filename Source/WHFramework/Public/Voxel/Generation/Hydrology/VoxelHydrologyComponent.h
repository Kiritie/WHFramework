#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyLakeCertificate.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyFineWaterQuery.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyRoutingGrid.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthRoutingQuery.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyRiverDecoration.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyBucketOutlet.h"

struct FVoxelGenerationRecipe;
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

struct WHFRAMEWORK_API FVoxelHydrologyReceivingVoxel
{
    int32 OriginalGroundZ = 0;
    int32 WaterZ = MIN_int32;
    FVoxelStableId BodyId;
    bool bOcean = false;
};

struct WHFRAMEWORK_API FVoxelHydrologyRiverComponent
{
    TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> ResourceLease;
    FIntPoint Key = FIntPoint::ZeroValue;
    uint64 Fingerprint = 0;
    uint64 RunoffFingerprint = 0;
    uint64 TotalRainWeight = 0, ReceivedRainWeight = 0;
    TMap<FIntPoint,FVoxelHydrologyBucketOutletResult> RunoffOutlets;
    TArray<FIntPoint> DrainageCells;
    FVoxelHydrologyPlan Plan;
    TMap<FIntPoint, int32> LakeWaterByVoxel;
    TMap<FIntPoint, FVoxelHydrologyReceivingVoxel> ReceivingWaterByVoxel;
    FVoxelHydrologyReceivingVoxel ExpandedReceivingWater;

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
    bool bRunoffTerminal = false, bParentRunoffTerminal = false;
    int32 DesiredWaterHalfWidth = 0;
    int32 DesiredDepth = 0;
    int32 ReceiverWaterZ = MIN_int32;
    FVoxelStableId ReceiverId;
    bool bHasParent = false;
    bool bSelected = false;
    bool bReceiver = false;
    bool bExpanded = false;
};

class WHFRAMEWORK_API FVoxelHydrologyComponentBuild
{
public:
    using FTileLookup = TFunctionRef<const FVoxelHydrologyPlan*(const FVoxelHydrologyRegionKey&)>;

    FVoxelHydrologyComponentBuild(
        TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
        TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
        const FIntPoint& InSeedCell);
    ~FVoxelHydrologyComponentBuild();

    EVoxelHydrologyComponentStep Advance(FTileLookup InLookup, int32 InWorkBudget,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);


    void BeginFineInputStep(const TAtomic<bool>* InCancel = nullptr);
    EVoxelHydrologyClosureRead ReadCertifiedFineCell(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologySharedCell& OutCell, FString& OutError);
    EVoxelHydrologyClosureRead ReadFineWater(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologyReceivingVoxel& OutWater, FString& OutError);
    const TArray<FVoxelHydrologyRegionKey>& GetMissingTiles() const;
    uint64 GetAllocatedBytes() const;
    const FVoxelHydrologyRiverComponent& GetResult() const;
    FVoxelHydrologyRiverComponent TakeResult();

    static bool CollectCoverageSeeds(const FVoxelHydrologyRegionKey& InCoverage,
        const FVoxelGenerationSettings& InSettings, FTileLookup InLookup,
        TArray<FIntPoint>& OutSeeds, TArray<FVoxelHydrologyRegionKey>& OutMissingTiles,
        FString& OutError);

    static bool CollectRegionSeeds(const FVoxelHydrologyRegionKey& InRegion,
        const FVoxelGenerationSettings& InSettings, FTileLookup InLookup,
        TArray<FIntPoint>& OutSeeds, TArray<FVoxelHydrologyRegionKey>& OutMissingTiles,
        FString& OutError);

private:
    bool InitializeRouting(FString& OutError);
    bool ReadRoutingFacts(const FIntPoint& InWorld, FTileLookup InLookup, FVoxelHydrologyComponentNode& OutNode, FString& OutError);
    EVoxelHydrologyClosureRead ReadRawFineCell(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologySharedCell& OutCell, FString& OutError);
    bool ReadRawNode(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologyComponentNode& OutNode, FString& OutError);
    bool ReadBucketOutlet(const FIntPoint& InAnchor, FTileLookup InLookup,
        FVoxelHydrologyBucketOutletResult& OutOutlet, FString& OutError);
    bool ReadRunoffNode(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologyComponentNode& OutNode, FString& OutError);
    bool ReadNode(const FIntPoint& InCell, FTileLookup InLookup,
        FVoxelHydrologyComponentNode& OutNode, FString& OutError, bool InRunoffTerminal = false);
    bool AddNode(const FVoxelHydrologyComponentNode& InNode, int32& OutIndex, FString& OutError);
    bool PrepareRiverNodes(FString& OutError, const TAtomic<bool>* InCancel);
    bool BuildGeometry(FTileLookup InLookup, FString& OutError, const TAtomic<bool>* InCancel);
    bool FinalizeCertifiedFootprint(FTileLookup InLookup, FString& OutError, const TAtomic<bool>* InCancel);
    bool ResolveProfiles(FTileLookup InLookup, FString& OutError, const TAtomic<bool>* InCancel);
    bool FinalizeProfileResult(FString& OutError, const TAtomic<bool>* InCancel);
    bool AddMissingTile(const FVoxelHydrologyRegionKey& InKey, FString& OutError);

private:
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> Recipe;
    TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> Terrain;
    FIntPoint SeedCell = FIntPoint::ZeroValue;
    FVoxelHydrologyRoutingGrid RoutingGrid;
    FVoxelHydrologyWidthRoutingQuery WidthRouting;
    FVoxelHydrologyRiverDecoration Decoration;
    bool bDecorationStarted = false, bDecorationReady = false;
    int32 DecorationBoundsCursor=0;
    FVoxelRiverShape RoutingShape;
    bool bCertifiedGeometryInitialized = false, bCertifiedGeometryRouteStarted = false;
    int32 CertifiedGeometryNode = 0, CertifiedGeometryRiver = INDEX_NONE, CertifiedGeometryPoint = 0;
    int32 CertifiedRiverSortStage = 0, CertifiedRiverSortIndex = 0, CertifiedRiverSortEnd = 0;
    bool bCertifiedRasterStarted = false;
    int32 CertifiedRasterRiver = 0, CertifiedRasterX = 0, CertifiedRasterY = 0;
    int32 CertifiedRasterInitPoint = 0, CertifiedRasterRadius = 0, CertifiedRasterSegment = 1, CertifiedRasterBestDistance = MAX_int32;
    FVoxelRiverProjection CertifiedRasterProjection;
    TArray<FIntPoint> CertifiedReceivingOrder;
    int32 CertifiedFinalStage = 0, CertifiedIndexRiver = 0, CertifiedIndexPoint = 0;
    int32 CertifiedFallStart = 0, CertifiedFallEnd = 1, CertifiedIndexCapacity = 0, CertifiedHashCursor = 0;
    FIntPoint CertifiedIndexMin, CertifiedIndexMax, CertifiedIndexCell;
    bool CertifiedIndexBucketActive = false;
    uint64 CertifiedHash = 0;
    bool bRoutingInitialized = false;
    bool bSeedMapped = false;
    bool bSeedInitialized = false;
    TMap<FIntPoint,uint64> RoutingRain;
    int32 RoutingRainCapacity = 0;
    bool bRoutingRainActive = false;
    FIntPoint RoutingRainSeed, RoutingRainNext, RoutingRainMin, RoutingRainMax;
    uint64 RoutingRainTotal = 0;
    struct FCanonicalDrainage
    {
        FIntPoint ParentCell = FIntPoint::ZeroValue;
        int32 SpillZ = 0;
        int32 PlateauDistance = 0;
        bool bHasParent = false;
        bool bParentRunoffTerminal = false;
    };
    FVoxelHydrologyBucketOutlet BucketOutlet;
    TMap<FIntPoint,FVoxelHydrologyBucketOutletResult> BucketOutlets;
    int32 BucketOutletCapacity=0, PublishedOutletCapacity=0;
    bool bBucketOutletActive=false;
    FIntPoint BucketOutletSeed=FIntPoint::ZeroValue;
    FVoxelHydrologyFineWaterQuery FineWaterQuery;
    TMap<FIntPoint,int32> RawGroundCache;
    TArray<FIntPoint> RawGroundRing;
    int32 RawGroundCapacity = 0, RawGroundEvict = 0;
    bool bRawGroundCacheDisabled = false;
    TMap<FIntPoint, FVoxelHydrologyReceivingVoxel> FineWaterCache;
    int32 FineWaterCapacity = 0;
    int32 FineWaterCopyIndex = 0;
    int32 FineDryCopyIndex = 0;
    FIntPoint FineWaterSeed = FIntPoint::ZeroValue;
    bool bFineWaterActive = false;
    FVoxelHydrologyLakeCertificate LakeCertificate;
    TMap<FIntPoint, FVoxelHydrologySharedCell> CertifiedFineRoots;
    int32 FineRootCapacity = 0;
    int32 LakeCertificateCopyIndex = 0;
    bool bLakeCertificateActive = false;
    FVoxelHydrologySharedSearch SharedSearch;
    TMap<FIntPoint, FCanonicalDrainage> CanonicalDrainage;
    int32 CanonicalCapacity = 0;
    int32 CertificateCopyIndex = 0;
    bool bSharedSearchActive = false;
    const TAtomic<bool>* ActiveCancel = nullptr;
    TArray<FVoxelHydrologyComponentNode> Nodes;
    TMap<FIntVector, int32> NodeByCell;
    TArray<FVoxelHydrologyRegionKey> MissingTiles;
    int32 NodeCapacity = 0;
    int32 MapCapacity = 0;
    int32 ExpandIndex = 0;
    TArray<int32> PrepareChildren, PrepareQueue, PrepareRemap, PrepareWinners;
    TArray<uint8> PrepareActive, PrepareQualifiedChild;
    TMap<FIntPoint,int32> PrepareBestSource;
    TArray<FVoxelHydrologyComponentNode> PrepareSelected;
    int32 PrepareStage = 0, PrepareCursor = 0, PrepareActiveCount = 0;
    int32 PrepareSortIndex = 0, PrepareSortEnd = 0;
    uint64 PrepareCoarseCount = 0;
    FIntPoint PrepareBucketMin, PrepareBucketMax, PrepareBucketNext;
    bool PrepareBucketActive = false;
    FVoxelHydrologyBasinClosure ProfileClosure;
    bool bProfileClosureActive = false;
    bool bProfileRawFallback = false;
    bool bProfilesSolved = false;
    int32 ProfileClosurePlane = MIN_int32;
    FVoxelStableId ProfileClosureReceiverId;
    FVoxelStableId AddedLakeReceiverId;
    bool bHasAddedLakeReceiver = false;
    int32 ReceivingVoxelCapacity = 0;
    FIntPoint ProfileClosureSeed = FIntPoint::ZeroValue;
    bool bNodesFiltered = false;
    bool bGeometryBuilt = false;
    bool bReady = false;
    FVoxelHydrologyRiverComponent Result;
};
