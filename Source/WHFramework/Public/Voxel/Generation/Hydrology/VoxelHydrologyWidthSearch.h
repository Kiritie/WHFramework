#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthTransfer.h"

struct WHFRAMEWORK_API FVoxelHydrologyWidthSearchEdge
{
    FIntPoint Downstream = FIntPoint::ZeroValue;
    FVoxelHydrologyWidthEdgeTransfer Transfer;
};

class WHFRAMEWORK_API FVoxelHydrologyWidthSearch
{
public:
    using FReadNode = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, FVoxelHydrologyWidthGraphNode&, FString&)>;
    using FReadEdge = TFunctionRef<EVoxelHydrologyClosureRead(const FIntPoint&, int32, FVoxelHydrologyWidthSearchEdge&, FString&)>;
    void Initialize(const FIntPoint& InSeed) { check(!bInitialized); Seed = InSeed; bInitialized = true; }
    EVoxelHydrologySharedSearchStep Advance(FReadNode ReadNode, FReadEdge ReadEdge, int32 WorkBudget,
        FString& OutError, const TAtomic<bool>* Cancel = nullptr);
    const FVoxelHydrologyWidthGraphLabel& GetSeedLabel() const { check(bReady); return SeedLabel; }
    const TArray<FVoxelHydrologyWidthGraphNode>& GetNodes() const { check(bReady); return Nodes; }
    const TArray<FVoxelHydrologyWidthEdgeTransfer>& GetEdges() const { check(bReady); return Edges; }
    int64 GetThreshold() const { check(bReady); return Threshold; }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    bool AppendNode(const FVoxelHydrologyWidthGraphNode& Node, int32& Index, FString& Error, int32 IncomingEdge = INDEX_NONE);
    FIntPoint Seed;
    int64 Threshold = 0;
    TArray<FVoxelHydrologyWidthGraphNode> Nodes;
    TArray<FVoxelHydrologyWidthEdgeTransfer> Edges;
    TArray<int32> DiscoveryEdge;
    TMap<FIntPoint,int32> ByCell;
    FVoxelHydrologyWidthGraphSolver Solver;
    FVoxelHydrologyWidthGraphLabel SeedLabel;
    int32 Cursor = 0, Face = 0, MapCapacity = 0;
    int32 WitnessNode = INDEX_NONE;
    int64 WitnessWater = 0;
    bool bInitialized = false, bSeedRead = false, bClosed = false, bReady = false;
};
