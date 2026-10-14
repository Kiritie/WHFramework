#pragma once

#include "Voxel/Generation/Hydrology/VoxelHydrologyRoutingGrid.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyJunctionCertificate.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthPort.h"

struct WHFRAMEWORK_API FVoxelHydrologyWidthRoutingLabel
{
    int64 W = 0;
    int64 D = 0;
    FIntPoint Parent = FIntPoint::ZeroValue;
    int32 ParentFace = INDEX_NONE;
    uint64 PathId = 0;
    uint64 FootprintVersion = 0;
    uint64 RecipeIdentity = 0;
    bool bEligible = false;
    bool bHasDistance = false;
    bool bHasParent = false;
};

struct WHFRAMEWORK_API FVoxelHydrologyWidthRoutingNode
{
    FIntPoint Position = FIntPoint::ZeroValue;
    FVoxelHydrologySectionInterval Interval;
    FIntPoint ConnectorEnd = FIntPoint::ZeroValue;
    bool bGridNode = false;
};

class WHFRAMEWORK_API FVoxelHydrologyWidthRoutingQuery
{
public:
    using FReadSample = FVoxelHydrologySectionCertificate::FReadSample;
    FVoxelHydrologyWidthRoutingQuery() = default;
    FVoxelHydrologyWidthRoutingQuery(const FVoxelHydrologyWidthRoutingQuery&) = delete;
    FVoxelHydrologyWidthRoutingQuery& operator=(const FVoxelHydrologyWidthRoutingQuery&) = delete;

    bool Initialize(const FVoxelHydrologyRoutingGrid& InGrid, const FVoxelRiverShape& InShape,
        int32 InMinimumGround, const FIntPoint& InWorldMin, const FIntPoint& InWorldMax,
        uint64 InRecipeIdentity, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(const FIntPoint& Seed, FReadSample ReadSample,
        int32 WorkBudget, FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    const FVoxelHydrologyWidthRoutingLabel* Find(const FIntPoint& Seed) const;
    const FVoxelHydrologyWidthRoutingNode* FindNode(const FIntPoint& Position) const;


    const FVoxelHydrologyWidthPort* FindSelectedPort(const FIntPoint& Seed) const;

    EVoxelHydrologySharedSearchStep AdvanceRestoreSelectedPort(const FIntPoint& Seed,
        int64 UpstreamW, int64 DownstreamW, int32 WorkBudget, FString& OutError,
        const TAtomic<bool>* Cancel = nullptr);
    bool IsRestoreReady() const { return bRestoreReady; }
    const TArray<int64>& GetRestoredHeights() const { check(bRestoreReady); return RestoredHeights; }

    uint64 GetAllocatedBytes() const;
    void FreeBacking();

private:
    struct FNodeEntry
    {
        FVoxelHydrologyWidthRoutingNode Certificate;
        FVoxelHydrologyWidthRoutingLabel Label;
        FVoxelHydrologyJunctionCertificate Junction;
        FVoxelHydrologySectionCertificate Section;
        int32 Ports[4] = { INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE };
        bool bStarted = false;
        bool bReady = false;
        bool bLabelReady = false;
    };
    struct FPortEntry
    {
        FVoxelHydrologyWidthPort Port;
        FVoxelHydrologyWidthSearchEdge Result;
        FIntPoint End = FIntPoint::ZeroValue;
        bool bStarted = false;
        bool bReady = false;
    };
    EVoxelHydrologySharedSearchStep EnsureNode(const FIntPoint& Position, FReadSample Read,
        int32& Budget, int32& OutIndex, FString& Error, const TAtomic<bool>* Cancel);
    EVoxelHydrologySharedSearchStep EnsurePort(const FIntPoint& Position, int32 Face,
        FReadSample Read, int32& Budget, FVoxelHydrologyWidthSearchEdge& Out,
        FString& Error, const TAtomic<bool>* Cancel);
    bool AddNode(const FIntPoint& Position, int32& OutIndex, FString& Error);
    bool PublishSeed(FString& Error);
    int32 SelectedPortIndex(const FIntPoint& Seed) const;

    FVoxelHydrologyRoutingGrid Grid;
    FVoxelRiverShape Shape;
    FIntPoint WorldMin = FIntPoint::ZeroValue;
    FIntPoint WorldMax = FIntPoint::ZeroValue;
    FIntPoint ActiveSeed = FIntPoint::ZeroValue;
    int32 MinimumGround = 0;
    int32 EqualityHalfLength = 0;
    int32 NodeMapCapacity = 0;
    uint64 RecipeIdentity = 0;
    TArray<FNodeEntry> Nodes;
    TMap<FIntPoint, int32> ByPosition;
    TArray<FPortEntry> Ports;
    FVoxelHydrologyWidthSearch Search;
    FVoxelHydrologyWidthRestoreState Restore;
    TArray<int64> RestoredHeights;
    FIntPoint RestoreSeed = FIntPoint::ZeroValue;
    int64 RestoreDownstreamW = 0, RestoreUpstreamW = 0, RestoreCurrentW = 0;
    int32 RestorePortIndex = INDEX_NONE;
    int32 RestoreInterval = 0;
    int32 RestoreRepeat = 0;
    bool bInitialized = false;
    bool bSearchActive = false;
    bool bRestoreStarted = false;
    bool bRestoreLogicalReady = false;
    bool bRestoreReady = false;
};
