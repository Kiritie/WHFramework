#pragma once
#include "Voxel/Generation/Hydrology/VoxelHydrologySharedSearch.h"

class WHFRAMEWORK_API FVoxelHydrologyLakeCertificate
{
public:
    using FReadCell = FVoxelHydrologySharedSearch::FReadCell;
    bool Initialize(const FVoxelHydrologySharedCell& InSeed, const FVoxelStableId& InOceanId,
        const FIntPoint& InMinimum, const FIntPoint& InMaximum, FString& OutError);
    EVoxelHydrologySharedSearchStep Advance(FReadCell InRead, int32 InBudget,
        FString& OutError, const TAtomic<bool>* InCancel = nullptr);
    bool IsReady() const { return bReady; }
    bool IsAccepted() const { check(bReady); return !bLowerContact && !bOpenBoundary; }
    int32 GetWaterZ() const { return WaterZ; }
    FVoxelStableId GetGroupId() const { check(bReady); return GroupId; }
    class WHFRAMEWORK_API FCellView
    {
    public:
        explicit FCellView(const FVoxelHydrologyLakeCertificate* InOwner) : Owner(InOwner) {}
        int32 Num() const { return Owner->Cells.Num(); }
        FVoxelHydrologySharedCell operator[](int32 InIndex) const;
    private:
        const FVoxelHydrologyLakeCertificate* Owner;
    };
    // 接受时为完整湖岸；拒绝时仅含已由真实排水/开放边界见证覆盖的连通前缀。
    FCellView GetCells() const { check(bReady); return FCellView(this); }
    uint64 GetAllocatedBytes() const;
    void FreeBacking();
private:
    bool Append(const FVoxelHydrologySharedCell& Cell, bool bWet, FString& OutError);
    bool HasSeen(const FIntPoint& InCell) const;
    bool MarkSeen(const FIntPoint& InCell, FString& OutError);
    void TraceState(bool bComplete);
    // 湖岸只消费坐标、原地面与接收体；不复制路径搜索的父图和出口详情。
    struct FShoreCell
    {
        FIntPoint Cell;
        int32 GroundZ;
        int32 ReceiverIndex = INDEX_NONE;
    };
    static_assert(sizeof(FShoreCell) <= 16);
    struct FReceiver
    {
        int32 WaterZ;
        FVoxelStableId Id;
        bool operator==(const FReceiver& Other) const { return WaterZ == Other.WaterZ && Id == Other.Id; }
        friend uint32 GetTypeHash(const FReceiver& Value)
        { return HashCombineFast(::GetTypeHash(Value.WaterZ), ::GetTypeHash(Value.Id)); }
    };
    struct FSeenTile
    {
        uint64 Bits[16] = {};
    };
    TVoxelGenerationPagedArray<FShoreCell> Cells;
    // 只对实际访问位设位；分配一块不代表整块坐标已访问或已认证。
    TVoxelGenerationPagedArray<FSeenTile, 64> SeenPages;
    TMap<FIntPoint, int32> SeenTiles;
    TArray<FReceiver> Receivers;
    TMap<FReceiver, int32> ReceiverIndices;
    int32 SeenCount = 0, SeenTileCapacity = 0, ReceiverCapacity = 0;
    int32 Cursor = 0;
    int32 Face = 0;
    int32 WaterZ = MIN_int32;
    FVoxelStableId GroupId, OceanId;
    FIntPoint Minimum, Maximum;
    FVoxelHydrologySharedCell Seed;
    bool bInitialized = false;
    bool bReady = false;
    bool bLowerContact = false;
    bool bOpenBoundary = false;
    double LastDiagnosticSeconds = 0.0;
};
