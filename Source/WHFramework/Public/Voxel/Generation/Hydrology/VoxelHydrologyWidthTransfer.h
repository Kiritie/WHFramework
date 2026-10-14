#pragma once
#include "CoreMinimal.h"


struct WHFRAMEWORK_API FVoxelHydrologyWidthNodeInterval
{
    int64 Lower = 0;
    int64 Upper = 0;
};
struct WHFRAMEWORK_API FVoxelHydrologyWidthEdgeTransfer
{
    int32 Upstream = INDEX_NONE;
    int32 Downstream = INDEX_NONE;
    int64 A = 0;
    int64 B = 0;
    int64 DownLower = 0;
    bool bHasRaise = false;
    bool bFeasible = false;
    uint64 PathId = 0;
    uint64 FootprintVersion = 0;
};
enum class EVoxelHydrologyWidthStep : uint8
{
    None,
    Pending,
    NeedInput,
    CapacityBlocked,
    Canceled,
    CertifiedInfeasible,
    InvalidInput,
    Ready
};

struct WHFRAMEWORK_API FVoxelHydrologyWidthComposeState
{
    void Reset();
    EVoxelHydrologyWidthStep Advance(const TArray<FVoxelHydrologyWidthNodeInterval>& Path,
        int32 WorkBudget, bool bInputComplete, bool bCancel,
        FVoxelHydrologyWidthEdgeTransfer& OutTransfer);
private:
    int32 Cursor = INDEX_NONE;
    int32 Count = 0;
    bool bStarted = false;
    EVoxelHydrologyWidthStep Status = EVoxelHydrologyWidthStep::Pending;
    FVoxelHydrologyWidthEdgeTransfer Candidate;
};

struct WHFRAMEWORK_API FVoxelHydrologyWidthRestoreState
{
    void Reset();
    EVoxelHydrologyWidthStep Advance(const TArray<FVoxelHydrologyWidthNodeInterval>& Path,
        int64 DownWater, int32 WorkBudget, bool bInputComplete, bool bCancel);
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
    const TArray<int64>& GetHeights() const { check(Status == EVoxelHydrologyWidthStep::Ready); return Heights; }
private:
    int32 Cursor = INDEX_NONE;
    int32 Count = 0;
    int64 Water = 0;
    int64 InputWater = 0;
    bool bStarted = false;
    EVoxelHydrologyWidthStep Status = EVoxelHydrologyWidthStep::Pending;
    TArray<int64> Heights;
    int32 ReverseCursor = 0;
};

struct WHFRAMEWORK_API FVoxelHydrologyWidthGraphNode
{
    FIntPoint Cell = FIntPoint::ZeroValue;
    FVoxelHydrologyWidthNodeInterval Interval;
    bool bFixed = false;
    int64 FixedH = 0;
};
struct WHFRAMEWORK_API FVoxelHydrologyWidthGraphLabel
{
    int64 W = 0;
    int64 D = 0;
    int32 Parent = INDEX_NONE;
    int32 ParentEdge = INDEX_NONE;
    bool bEligible = false;
    bool bHasDistance = false;
};

class WHFRAMEWORK_API FVoxelHydrologyWidthGraphSolver
{
public:
    void Reset();
    EVoxelHydrologyWidthStep Advance(const TArray<FVoxelHydrologyWidthGraphNode>& Nodes,
        const TArray<FVoxelHydrologyWidthEdgeTransfer>& Edges,
        bool bDomainClosed, int32 WorkBudget, bool bCancel = false);
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
    bool IsReady() const { return Status == EVoxelHydrologyWidthStep::Ready; }
    const TArray<FVoxelHydrologyWidthGraphLabel>& GetLabels() const { check(IsReady()); return Labels; }
private:
    bool Tight(const FVoxelHydrologyWidthEdgeTransfer& Edge) const;
    bool BetterParent(const TArray<FVoxelHydrologyWidthGraphNode>& Nodes,
        const TArray<FVoxelHydrologyWidthEdgeTransfer>& Edges, int32 EdgeIndex, int32 OldIndex) const;
    bool HeapLess(int32 A, int32 B, const TArray<FVoxelHydrologyWidthGraphNode>& Nodes) const;
    void HeapUpdate(int32 Node, const TArray<FVoxelHydrologyWidthGraphNode>& Nodes);
    int32 HeapPop(const TArray<FVoxelHydrologyWidthGraphNode>& Nodes);
    TArray<FVoxelHydrologyWidthGraphLabel> Labels;
    TArray<int32> Heap;
    TArray<int32> Positions;
    TArray<int32> IncomingHead;
    TArray<int32> IncomingNext;
    int32 NodeCount = 0;
    int32 EdgeCount = 0;
    int32 Cursor = 0;
    int32 Current = INDEX_NONE;
    int32 CurrentEdge = INDEX_NONE;
    int32 PreviousCoordinate = INDEX_NONE;
    uint8 Stage = 0;
    uint8 HeapMode = 0;
    EVoxelHydrologyWidthStep Status = EVoxelHydrologyWidthStep::Pending;
};
