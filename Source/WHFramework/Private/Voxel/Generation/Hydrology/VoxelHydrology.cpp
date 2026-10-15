#include "Voxel/Generation/Hydrology/VoxelHydrology.h"

#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

namespace
{
    int32 NextHydrologyCapacity(const int32 InRequired, const int32 InCapacity)
    {
        return int32(FMath::Min<int64>(MAX_int32,
            FMath::Max<int64>(InRequired, FMath::Max<int64>(4, int64(InCapacity) * 2))));
    }

    bool GetWorldHydrologyCore(const FVoxelHydrologyRegionKey& InKey, const FVoxelGenerationSettings& Settings,
        FInt64Vector2& OutMin, FInt64Vector2& OutMax)
    {
        if (Settings.HydrologyRegionSide <= 0 || Settings.HydrologyCellSize <= 0) return false;
        const int64 DomainMin = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, Settings.HydrologyCellSize);
        const int64 DomainMax = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, Settings.HydrologyCellSize) + 1;
        const FInt64Vector2 Min(int64(InKey.Coordinate.X) * Settings.HydrologyRegionSide,
            int64(InKey.Coordinate.Y) * Settings.HydrologyRegionSide);
        const FInt64Vector2 ClippedMin(FMath::Max(Min.X, DomainMin), FMath::Max(Min.Y, DomainMin));
        const FInt64Vector2 ClippedMax(FMath::Min(Min.X + Settings.HydrologyRegionSide, DomainMax),
            FMath::Min(Min.Y + Settings.HydrologyRegionSide, DomainMax));
        if (ClippedMin.X >= ClippedMax.X || ClippedMin.Y >= ClippedMax.Y) return false;
        OutMin = ClippedMin;
        OutMax = ClippedMax;
        return true;
    }

    constexpr int32 HydrologyMinPlane = -32768;
    constexpr int32 HydrologyMaxPlane = 32768;

    struct FResolvedRiverSize
    {
        int32 BedHalfWidth = 1;
        int32 WaterHalfWidth = 1;
        int32 BankWidth = 1;
        int32 ShoreWidth = 1;
        int32 Depth = 1;
    };

    FResolvedRiverSize ResolveRiverSize(const uint64 InAccumulation,
        const FVoxelGenerationSettings& InSettings)
    {
        const uint64 Threshold = FMath::Max<uint64>(1,
            InSettings.RiverSourceAccumulation);
        const uint64 Relative = FMath::Max<uint64>(1,
            InAccumulation / Threshold);
        const int32 GrowthLevel = FMath::Clamp(
            static_cast<int32>(FMath::FloorLog2(
                static_cast<uint32>(FMath::Min<uint64>(Relative, MAX_uint32)))),
            0, InSettings.RiverMaxGrowthLevels);
        FResolvedRiverSize Result;
        Result.BedHalfWidth = InSettings.RiverBaseHalfWidth +
            GrowthLevel * InSettings.RiverWidthGrowthPerLevel;
        Result.WaterHalfWidth = Result.BedHalfWidth;
        Result.BankWidth = InSettings.RiverBankWidth;
        Result.ShoreWidth = FMath::Max(Result.BankWidth + 1,
            InSettings.RiverShoreWidth);
        Result.Depth = InSettings.RiverBaseDepth +
            GrowthLevel * InSettings.RiverDepthGrowthPerLevel;
        return Result;
    }

    constexpr int32 HydrologyDirectionX[4] =
    {
        1,
        -1,
        0,
        0
    };

    constexpr int32 HydrologyDirectionY[4] =
    {
        0,
        0,
        1,
        -1
    };

    bool IsCanceled(
        const TAtomic<bool>* InCancel)
    {
        return InCancel &&
            InCancel->Load();
    }

    bool ValidateGridCount(
        int32 InWidth,
        int32 InHeight,
        int32& OutCount)
    {
        if (InWidth < 3 ||
            InHeight < 3 ||
            InWidth > 4096 ||
            InHeight > 4096)
        {
            return false;
        }

        const int64 Count =
            static_cast<int64>(InWidth) *
            static_cast<int64>(InHeight);

        if (Count <= 0 ||
            Count > MAX_int32)
        {
            return false;
        }

        OutCount =
            static_cast<int32>(Count);

        return true;
    }

    bool TryGetNeighbor(
        uint32 InIndex,
        uint8 InFace,
        int32 InWidth,
        int32 InHeight,
        uint32& OutNeighbor)
    {
        if (InFace >= 4)
        {
            return false;
        }

        const int32 CurrentX =
            static_cast<int32>(InIndex %
                static_cast<uint32>(InWidth));

        const int32 CurrentY =
            static_cast<int32>(InIndex /
                static_cast<uint32>(InWidth));

        const int32 NeighborX =
            CurrentX +
            HydrologyDirectionX[InFace];

        const int32 NeighborY =
            CurrentY +
            HydrologyDirectionY[InFace];

        if (NeighborX < 0 ||
            NeighborY < 0 ||
            NeighborX >= InWidth ||
            NeighborY >= InHeight)
        {
            return false;
        }

        OutNeighbor =
            static_cast<uint32>(
                NeighborX +
                NeighborY *
                InWidth);

        return true;
    }

    struct FHydrologyHeapEntry
    {
        int32 SpillPlane = 0;
        uint32 Index = 0;
        FIntPoint WorldCell = FIntPoint::ZeroValue;
    };

    FIntPoint ResolveWorldCell(
        const FVoxelDrainageInput& InInput,
        uint32 InIndex)
    {
        return InInput.WorldMinCell +
            FIntPoint(
                static_cast<int32>(
                    InIndex %
                    static_cast<uint32>(
                        InInput.Width)),
                static_cast<int32>(
                    InIndex /
                    static_cast<uint32>(
                        InInput.Width)));
    }

    bool IsWorldCellLess(
        const FIntPoint& InA,
        const FIntPoint& InB)
    {
        if (InA.Y != InB.Y)
        {
            return InA.Y < InB.Y;
        }

        return InA.X < InB.X;
    }

    bool HydrologyHeapLess(
        const FHydrologyHeapEntry& InA,
        const FHydrologyHeapEntry& InB)
    {
        if (InA.SpillPlane !=
            InB.SpillPlane)
        {
            return InA.SpillPlane <
                InB.SpillPlane;
        }

        return IsWorldCellLess(
            InA.WorldCell,
            InB.WorldCell);
    }

    bool HydrologyHeapPush(
        TArray<FHydrologyHeapEntry>& InOutHeap,
        const FHydrologyHeapEntry& InEntry,
        FVoxelGenerationWorkspace& InWorkspace,
        FString& OutError)
    {
        if (InOutHeap.Num() == InOutHeap.Max() && !InWorkspace.ReserveArray(InOutHeap,
            NextHydrologyCapacity(InOutHeap.Num() + 1, InOutHeap.Max()), OutError))
        {
            return false;
        }
        int32 Child =
            InOutHeap.Add(InEntry);

        while (Child > 0)
        {
            const int32 Parent =
                (Child - 1) / 2;

            if (!HydrologyHeapLess(
                InOutHeap[Child],
                InOutHeap[Parent]))
            {
                break;
            }

            Swap(
                InOutHeap[Child],
                InOutHeap[Parent]);

            Child = Parent;
        }
        return true;
    }

    bool HydrologyHeapPop(
        TArray<FHydrologyHeapEntry>& InOutHeap,
        FHydrologyHeapEntry& OutEntry)
    {
        if (InOutHeap.IsEmpty())
        {
            return false;
        }

        OutEntry =
            InOutHeap[0];

        if (InOutHeap.Num() == 1)
        {
            InOutHeap.Reset();
            return true;
        }

        InOutHeap[0] =
            InOutHeap.Last();

        InOutHeap.SetNum(
            InOutHeap.Num() - 1,
            EAllowShrinking::No);

        int32 Parent = 0;

        while (true)
        {
            const int32 Left =
                Parent * 2 + 1;

            if (Left >=
                InOutHeap.Num())
            {
                break;
            }

            const int32 Right =
                Left + 1;

            int32 Best = Left;

            if (Right <
                    InOutHeap.Num() &&
                HydrologyHeapLess(
                    InOutHeap[Right],
                    InOutHeap[Left]))
            {
                Best = Right;
            }

            if (!HydrologyHeapLess(
                InOutHeap[Best],
                InOutHeap[Parent]))
            {
                break;
            }

            Swap(
                InOutHeap[Best],
                InOutHeap[Parent]);

            Parent = Best;
        }

        return true;
    }

    int32 SmoothQ16(
        int32 InAlphaQ16)
    {
        const int64 X =
            FMath::Clamp(
                InAlphaQ16,
                0,
                65536);

        const int64 X2 =
            X * X /
            65536;

        const int64 X3 =
            X2 * X /
            65536;

        return static_cast<int32>(
            3 * X2 -
            2 * X3);
    }
}

bool VoxelHydrology::BuildDrainage(
    const FVoxelDrainageInput& InInput,
    FVoxelDrainageResult& OutResult,
    FString& OutError,
    const TAtomic<bool>* InCancel)
{
    if (IsCanceled(InCancel))
    {
        OutError = TEXT("Canceled");
        return false;
    }

    int32 Count = 0;

    if (!ValidateGridCount(
            InInput.Width,
            InInput.Height,
            Count) ||
        InInput.GroundPlane.Num() != Count ||
        InInput.RainWeight.Num() != Count ||
        InInput.Allowed.Num() != Count ||
        int64(InInput.WorldMinCell.X) + InInput.Width > VoxelGeneration::WorldCellMaxExclusive ||
        int64(InInput.WorldMinCell.Y) + InInput.Height > VoxelGeneration::WorldCellMaxExclusive ||
        InInput.Outlets.IsEmpty())
    {
        OutError =
            TEXT("Invalid drainage arrays/outlets");

        return false;
    }

    for (int32 Index = 0;
        Index < Count;
        ++Index)
    {
        if (InInput.GroundPlane[Index] <
                HydrologyMinPlane ||
            InInput.GroundPlane[Index] >
                HydrologyMaxPlane ||
            InInput.RainWeight[Index] >
                10000 ||
            InInput.Allowed[Index] > 1)
        {
            OutError =
                TEXT("Drainage value outside bounds");

            return false;
        }
    }

    FVoxelDrainageResult Result;
    if (!VoxelGenerationAllocation::RetainArray(Result.SpillPlane, Count, OutError) ||
        !VoxelGenerationAllocation::RetainArray(Result.Parent, Count, OutError) ||
        !VoxelGenerationAllocation::RetainArray(Result.Accumulation, Count, OutError) ||
        !VoxelGenerationAllocation::RetainArray(Result.SettledOrder, Count, OutError))
    {
        return false;
    }

    Result.SpillPlane.Init(
        MAX_int32,
        Count);

    Result.Parent.Init(
        -2,
        Count);

    Result.Accumulation.Init(
        0,
        Count);

    FVoxelGenerationWorkspace Workspace;
    TArray<uint8> Settled;
    TArray<uint8> Terminal;
    TArray<FHydrologyHeapEntry> Heap;
    TArray<uint32> SortedOutletIndices;
    if (!Workspace.ReserveArray(Settled, Count, OutError) ||
        !Workspace.ReserveArray(Terminal, Count, OutError) ||
        !Workspace.ReserveArray(Heap, InInput.Outlets.Num(), OutError) ||
        !Workspace.ReserveArray(SortedOutletIndices, InInput.Outlets.Num(), OutError))
    {
        return false;
    }
    Settled.Init(
        0,
        Count);

    Terminal.Init(
        0,
        Count);

    for (const auto& Pair : InInput.Outlets)
    {
        SortedOutletIndices.Add(Pair.Key);
    }

    SortedOutletIndices.Sort();

    for (uint32 Index :
        SortedOutletIndices)
    {
        const int32* WaterPlane =
            InInput.Outlets.Find(Index);

        if (!WaterPlane ||
            Index >=
                static_cast<uint32>(Count) ||
            InInput.Allowed[Index] == 0 ||
            *WaterPlane <
                HydrologyMinPlane ||
            *WaterPlane >
                HydrologyMaxPlane)
        {
            OutError =
                TEXT("Invalid outlet");

            return false;
        }

        Terminal[Index] = 1;

        Result.SpillPlane[Index] =
            FMath::Max(
                InInput.GroundPlane[Index],
                *WaterPlane);

        Result.Parent[Index] = -1;

        if (!HydrologyHeapPush(
            Heap,
            {
                Result.SpillPlane[Index],
                Index,
                ResolveWorldCell(
                    InInput,
                    Index)
            }, Workspace, OutError))
        {
            return false;
        }
    }

    FHydrologyHeapEntry Entry;

    while (HydrologyHeapPop(
        Heap,
        Entry))
    {
        if (IsCanceled(InCancel))
        {
            OutError = TEXT("Canceled");
            return false;
        }

        const uint32 Index =
            Entry.Index;

        if (Settled[Index] != 0 ||
            Entry.SpillPlane !=
                Result.SpillPlane[Index])
        {
            continue;
        }

        Settled[Index] = 1;

        Result.SettledOrder.Add(
            Index);

        Result.Accumulation[Index] =
            InInput.RainWeight[Index];

        for (uint8 Face = 0;
            Face < 4;
            ++Face)
        {
            uint32 Neighbor = 0;

            if (!TryGetNeighbor(
                    Index,
                    Face,
                    InInput.Width,
                    InInput.Height,
                    Neighbor) ||
                InInput.Allowed[Neighbor] == 0 ||
                Settled[Neighbor] != 0 ||
                Terminal[Neighbor] != 0)
            {
                continue;
            }

            const int32 Candidate =
                FMath::Max(
                    Entry.SpillPlane,
                    InInput.GroundPlane[Neighbor]);

            if (Candidate <
                Result.SpillPlane[Neighbor])
            {
                Result.SpillPlane[Neighbor] =
                    Candidate;

                Result.Parent[Neighbor] =
                    static_cast<int32>(Index);

                if (!HydrologyHeapPush(
                    Heap,
                    {
                        Candidate,
                        Neighbor,
                        ResolveWorldCell(
                            InInput,
                            Neighbor)
                    }, Workspace, OutError))
                {
                    return false;
                }
            }

            /**
             * Equal Candidate 不改 Parent。
             *
             * 因为 Heap tie-break 固定为
             * (SpillPlane, Index)，
             * 首个 settled predecessor
             * 是稳定且无环的。
             */
        }
    }

    for (int32 Index = 0;
        Index < Count;
        ++Index)
    {
        if (InInput.Allowed[Index] != 0 &&
            Settled[Index] == 0)
        {
            OutError =
                TEXT(
                    "Allowed drainage component "
                    "has no certified outlet");

            return false;
        }
    }

    for (int32 OrderIndex =
            Result.SettledOrder.Num() - 1;
        OrderIndex >= 0;
        --OrderIndex)
    {
        const uint32 Index =
            Result.SettledOrder[OrderIndex];

        const int32 Parent =
            Result.Parent[Index];

        if (Parent >= 0)
        {
            Result.Accumulation[Parent] +=
                Result.Accumulation[Index];
        }
    }

    VoxelGenerationAllocation::FreeRetainedArray(OutResult.SpillPlane);
    VoxelGenerationAllocation::FreeRetainedArray(OutResult.Parent);
    VoxelGenerationAllocation::FreeRetainedArray(OutResult.Accumulation);
    VoxelGenerationAllocation::FreeRetainedArray(OutResult.SettledOrder);
    OutResult =
        MoveTemp(Result);

    OutError.Reset();
    return true;
}

bool VoxelHydrology::TraceDrainage(
    const FVoxelDrainageInput& InInput,
    const FVoxelDrainageResult& InResult,
    uint32 InSource,
    int32 InRouteBudget,
    TArray<uint32>& OutPath,
    FString& OutError,
    FVoxelGenerationWorkspace* InOutputWorkspace)
{
    int32 Count = 0;

    if (!ValidateGridCount(
            InInput.Width,
            InInput.Height,
            Count) ||
        InSource >=
            static_cast<uint32>(Count) ||
        InInput.Allowed.Num() != Count ||
        InInput.Allowed[InSource] == 0 ||
        InResult.Parent.Num() != Count ||
        InResult.SpillPlane.Num() != Count ||
        InRouteBudget <= 0)
    {
        OutError =
            TEXT("Invalid route request");

        return false;
    }

    TArray<uint32> Path;
    const int32 PathCapacity = FMath::Min(InRouteBudget, Count);
    if (!(InOutputWorkspace ? InOutputWorkspace->ReserveArray(Path, PathCapacity, OutError) :
        VoxelGenerationAllocation::RetainArray(Path, PathCapacity, OutError)))
    {
        return false;
    }

    FVoxelGenerationWorkspace Workspace;
    TArray<uint8> Seen;
    if (!Workspace.ReserveArray(Seen, Count, OutError))
    {
        return false;
    }
    Seen.Init(
        0,
        Count);

    uint32 Index =
        InSource;

    while (true)
    {
        if (Index >=
                static_cast<uint32>(Count) ||
            Seen[Index] != 0 ||
            Path.Num() >=
                InRouteBudget)
        {
            OutError =
                TEXT(
                    "Drainage cycle or route "
                    "budget exceeded");

            return false;
        }

        Seen[Index] = 1;
        Path.Add(Index);

        const int32 Parent =
            InResult.Parent[Index];

        if (Parent == -1)
        {
            if (!InInput.Outlets.Contains(
                Index))
            {
                OutError =
                    TEXT(
                        "Uncertified route terminal");

                return false;
            }

            break;
        }

        if (Parent < 0 ||
            Parent >= Count ||
            InResult.SpillPlane[Parent] >
                InResult.SpillPlane[Index])
        {
            OutError =
                TEXT("Invalid upstream parent");

            return false;
        }

        const int32 CurrentX =
            static_cast<int32>(
                Index %
                static_cast<uint32>(
                    InInput.Width));

        const int32 CurrentY =
            static_cast<int32>(
                Index /
                static_cast<uint32>(
                    InInput.Width));

        const int32 ParentX =
            Parent %
            InInput.Width;

        const int32 ParentY =
            Parent /
            InInput.Width;

        if (FMath::Abs(
                ParentX -
                CurrentX) +
            FMath::Abs(
                ParentY -
                CurrentY) != 1)
        {
            OutError =
                TEXT(
                    "Nonadjacent drainage parent");

            return false;
        }

        Index =
            static_cast<uint32>(Parent);
    }

    if (InOutputWorkspace)
    {
        InOutputWorkspace->FreeArray(OutPath);
        OutPath = MoveTemp(Path);
    }
    else if (!VoxelGenerationAllocation::MoveRetainedArray(OutPath, Path, OutError))
    {
        return false;
    }

    OutError.Reset();
    return true;
}

bool VoxelHydrology::ValidateBasin(
    const FVoxelBasinInput& InInput,
    FVoxelBasinCertificate& OutCertificate,
    FString& OutError,
    const TAtomic<bool>* InCancel)
{
    if (IsCanceled(InCancel))
    {
        OutError = TEXT("Canceled");
        return false;
    }

    int32 Count = 0;

    if (!ValidateGridCount(
            InInput.Width,
            InInput.Height,
            Count) ||
        InInput.GroundPlane.Num() != Count ||
        InInput.Known.Num() != Count ||
        InInput.WaterMask.Num() != Count ||
        InInput.WaterPlane <
            HydrologyMinPlane ||
        InInput.WaterPlane >
            HydrologyMaxPlane)
    {
        OutError =
            TEXT("Invalid basin arrays");

        return false;
    }

    for (const TPair<
        FVoxelBasinConnectorKey,
        int32>& Pair :
        InInput.ConnectorPlanes)
    {
        if (Pair.Key.CellIndex >=
                static_cast<uint32>(Count) ||
            Pair.Key.Face >= 4 ||
            Pair.Value !=
                InInput.WaterPlane)
        {
            OutError =
                TEXT(
                    "Invalid/mismatched "
                    "water connector");

            return false;
        }
    }

    FVoxelBasinCertificate Certificate;

    uint32 FirstWaterCell = 0;
    bool bHasFirstWaterCell = false;

    for (uint32 Index = 0;
        Index <
            static_cast<uint32>(Count);
        ++Index)
    {
        if ((Index & 255u) == 0u &&
            IsCanceled(InCancel))
        {
            OutError = TEXT("Canceled");
            return false;
        }

        if (InInput.Known[Index] > 1 ||
            InInput.WaterMask[Index] > 1)
        {
            OutError =
                TEXT("Invalid basin flags");

            return false;
        }

        if (InInput.WaterMask[Index] == 0)
        {
            continue;
        }

        if (!bHasFirstWaterCell)
        {
            FirstWaterCell = Index;
            bHasFirstWaterCell = true;
        }

        ++Certificate.WaterCells;

        if (InInput.Known[Index] == 0 ||
            InInput.GroundPlane[Index] >=
                InInput.WaterPlane)
        {
            OutError =
                TEXT(
                    "Unknown or dry cell "
                    "in water footprint");

            return false;
        }

        for (uint8 Face = 0;
            Face < 4;
            ++Face)
        {
            uint32 Neighbor = 0;

            const bool bHasNeighbor =
                TryGetNeighbor(
                    Index,
                    Face,
                    InInput.Width,
                    InInput.Height,
                    Neighbor);

            if (bHasNeighbor &&
                InInput.WaterMask[Neighbor] != 0)
            {
                continue;
            }

            if (bHasNeighbor &&
                InInput.Known[Neighbor] != 0 &&
                InInput.GroundPlane[Neighbor] >=
                    InInput.WaterPlane)
            {
                ++Certificate.ClosedEdges;
                continue;
            }

            const FVoxelBasinConnectorKey
                ConnectorKey
                {
                    Index,
                    Face
                };

            const int32* ConnectorPlane =
                InInput.ConnectorPlanes.Find(
                    ConnectorKey);

            if (ConnectorPlane)
            {
                /**
                 * ConnectorPlane 在前面已经验证
                 * == WaterPlane。
                 * 这里不猜测外部水体，只接受调用者
                 * 明确认证过的 Connector。
                 */
                ++Certificate.ConnectorEdges;
                continue;
            }

            OutError =
                bHasNeighbor &&
                InInput.Known[Neighbor] == 0
                ? TEXT(
                    "Unknown shore cannot "
                    "certify a lake")
                : TEXT(
                    "Lake footprint leaks "
                    "through final shore");

            return false;
        }
    }

    if (Certificate.WaterCells == 0)
    {
        OutError =
            TEXT(
                "Empty basin is not "
                "a lake certificate");

        return false;
    }

    FVoxelGenerationWorkspace Workspace;
    TArray<uint8> Seen;
    TArray<uint32> Queue;
    if (!Workspace.ReserveArray(Seen, Count, OutError) ||
        !Workspace.ReserveArray(Queue, int32(Certificate.WaterCells), OutError))
    {
        return false;
    }
    Seen.Init(
        0,
        Count);

    Queue.Add(
        FirstWaterCell);

    Seen[FirstWaterCell] = 1;

    for (int32 QueueIndex = 0;
        QueueIndex < Queue.Num();
        ++QueueIndex)
    {
        if ((QueueIndex & 255) == 0 &&
            IsCanceled(InCancel))
        {
            OutError = TEXT("Canceled");
            return false;
        }

        const uint32 Index =
            Queue[QueueIndex];

        for (uint8 Face = 0;
            Face < 4;
            ++Face)
        {
            uint32 Neighbor = 0;

            if (TryGetNeighbor(
                    Index,
                    Face,
                    InInput.Width,
                    InInput.Height,
                    Neighbor) &&
                InInput.WaterMask[Neighbor] != 0 &&
                Seen[Neighbor] == 0)
            {
                Seen[Neighbor] = 1;
                Queue.Add(Neighbor);
            }
        }
    }

    if (static_cast<uint32>(
            Queue.Num()) !=
        Certificate.WaterCells)
    {
        OutError =
            TEXT(
                "Disconnected footprint must "
                "be separate lake bodies");

        return false;
    }

    OutCertificate =
        Certificate;

    OutError.Reset();
    return true;
}

bool VoxelHydrology::EvaluateRiverSection(
    int32 InOriginalGround,
    int32 InWaterPlane,
    int32 InDistanceCells,
    const FVoxelRiverShape& InShape,
    FVoxelRiverSection& OutSection,
    FString& OutError)
{
    if (InOriginalGround < HydrologyMinPlane ||
        InOriginalGround > HydrologyMaxPlane ||
        InWaterPlane < HydrologyMinPlane ||
        InWaterPlane > HydrologyMaxPlane - 1 ||
        InDistanceCells < 0 || InDistanceCells > 65536 ||
        InShape.BedHalfWidth < 1 || InShape.BedHalfWidth > 256 ||
        InShape.WaterHalfWidth < InShape.BedHalfWidth ||
        InShape.WaterHalfWidth > 256 ||
        InShape.BankWidth < 1 || InShape.BankWidth > 512 ||
        InShape.ShoreWidth < 1 || InShape.ShoreWidth > 1024 ||
        InShape.Depth < 1 || InShape.Depth > 256 ||
        InShape.MaxCutFill < 1 || InShape.MaxCutFill > 512 ||
        InWaterPlane - InShape.Depth < HydrologyMinPlane)
    {
        OutError = TEXT("Invalid river section");
        return false;
    }
    FVoxelRiverSection Section;
    Section.WaterPlane = InWaterPlane;
    Section.BedPlane = InWaterPlane - InShape.Depth;
    const int32 WetEnd = InShape.WaterHalfWidth;
    const int32 BankEnd = WetEnd + InShape.BankWidth;
    const int32 ShoreEnd = BankEnd + InShape.ShoreWidth;
    auto Blend = [](const int32 A, const int32 B,
        const int32 Position, const int32 Start, const int32 End)
    {
        const int32 Alpha = static_cast<int32>(
            static_cast<int64>(Position - Start) * 65536 /
            FMath::Max(1, End - Start));
        return A + static_cast<int32>(
            static_cast<int64>(B - A) * SmoothQ16(Alpha) / 65536);
    };
    if (InDistanceCells <= InShape.BedHalfWidth)
    {
        Section.GroundPlane = Section.BedPlane;
        Section.Zone = EVoxelRiverSurfaceZone::ChannelBed;
        Section.bWet = true;
    }
    else if (InDistanceCells <= WetEnd)
    {
        Section.GroundPlane = Blend(Section.BedPlane, InWaterPlane - 1,
            InDistanceCells, InShape.BedHalfWidth, WetEnd);
        Section.Zone = EVoxelRiverSurfaceZone::WetMargin;
        Section.bWet = true;
    }
    else if (InDistanceCells <= BankEnd)
    {
        const int32 TargetGround = FMath::Max(InWaterPlane,
            FMath::Min(InOriginalGround, InWaterPlane + InShape.Depth / 2));
        Section.GroundPlane = Blend(InWaterPlane, TargetGround,
            InDistanceCells, WetEnd, BankEnd);
        Section.Zone = EVoxelRiverSurfaceZone::DryBank;
    }
    else if (InDistanceCells <= ShoreEnd)
    {
        const int32 StartGround = FMath::Min(InOriginalGround,
            InWaterPlane + InShape.Depth / 2);
        Section.GroundPlane = Blend(StartGround, InOriginalGround,
            InDistanceCells, BankEnd, ShoreEnd);
        Section.Zone = EVoxelRiverSurfaceZone::Floodplain;
    }
    else
    {
        Section.GroundPlane = InOriginalGround;
    }
    const int64 Delta = static_cast<int64>(Section.GroundPlane) - InOriginalGround;
    if (FMath::Abs(Delta) > InShape.MaxCutFill)
    {
        Section.bExceededCutFill = true;
        if (!Section.bWet)
        {
            Section.GroundPlane = InOriginalGround +
                static_cast<int32>(FMath::Clamp<int64>(Delta,
                    -InShape.MaxCutFill, InShape.MaxCutFill));
        }
    }
    OutSection = Section;
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGrid::IsValidIndex(
    uint32 InIndex) const
{
    return Width > 0 && Height > 0 && uint64(InIndex) < uint64(Width) * uint64(Height);
}

uint32 FVoxelHydrologyGrid::ToIndex(
    int32 InX,
    int32 InY) const
{
    check(InX >= 0);
    check(InY >= 0);
    check(InX < Width);
    check(InY < Height);

    const int64 Index = int64(InX) + int64(InY) * Width;
    check(Index <= MAX_uint32);
    return uint32(Index);
}

FIntPoint FVoxelHydrologyGrid::ToLocal(
    uint32 InIndex) const
{
    check(IsValidIndex(InIndex));

    return FIntPoint(
        static_cast<int32>(
            InIndex %
            static_cast<uint32>(Width)),
        static_cast<int32>(
            InIndex /
            static_cast<uint32>(Width)));
}

FIntPoint FVoxelHydrologyGrid::ToWorldHydrologyCell(
    uint32 InIndex) const
{
    return WorldMinCell +
        ToLocal(InIndex);
}

bool FVoxelHydrologyGrid::TryToWorldVoxelXY(const uint32 InIndex, FIntPoint& OutPoint) const
{
    if (Width <= 0 || Height <= 0 || CellSize <= 0 || uint64(InIndex) >= uint64(Width) * uint64(Height)) return false;
    const FIntPoint Local = ToLocal(InIndex);
    const int64 X = (int64(WorldMinCell.X) + Local.X) * CellSize;
    const int64 Y = (int64(WorldMinCell.Y) + Local.Y) * CellSize;
    if (X + CellSize <= VoxelGeneration::WorldCellMin || Y + CellSize <= VoxelGeneration::WorldCellMin) return false;
    // 边界格点只采自己的非空 cell 交集，不创建域外 halo 或复制边缘样本。
    return VoxelGeneration::TryWorldPoint(FInt64Vector2(FMath::Max(X, VoxelGeneration::WorldCellMin),
        FMath::Max(Y, VoxelGeneration::WorldCellMin)), OutPoint);
}

FVoxelHydrologyGenerator::FVoxelHydrologyGenerator(
    TSharedRef<
        const FVoxelGenerationRecipe,
        ESPMode::ThreadSafe> InRecipe,
    TSharedRef<
        const FVoxelTerrainGenerator,
        ESPMode::ThreadSafe> InTerrain)
    : Recipe(MoveTemp(InRecipe))
    , Terrain(MoveTemp(InTerrain))
{
}

bool FVoxelHydrologyGenerator::BuildHeightGrid(
    const FVoxelHydrologyRegionKey& InKey,
    FVoxelHydrologyGrid& OutGrid,
    FString& OutError,
    const TAtomic<bool>* InCancel) const
{
    const FVoxelGenerationSettings& Settings =
        Recipe->Settings;

    const int32 RegionSide =
        Settings.HydrologyRegionSide;

    const int32 Halo =
        Settings.HydrologyHaloCells;

    const int64 GridSide = int64(RegionSide) + int64(Halo) * 2;
    FInt64Vector2 CoreMin, CoreMax;
    if (Halo < 0 || !GetWorldHydrologyCore(InKey, Settings, CoreMin, CoreMax) || GridSide <= 0 ||
        GridSide > 4096)
    {
        OutError =
            TEXT("Voxel hydrology grid side is invalid");

        return false;
    }

    const int64 DomainMin = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, Settings.HydrologyCellSize);
    const int64 DomainMax = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, Settings.HydrologyCellSize) + 1;
    const FInt64Vector2 GridMin(FMath::Max(CoreMin.X - Halo, DomainMin), FMath::Max(CoreMin.Y - Halo, DomainMin));
    const FInt64Vector2 GridMax(FMath::Min(CoreMax.X + Halo, DomainMax), FMath::Min(CoreMax.Y + Halo, DomainMax));
    FVoxelHydrologyGrid Grid;

    Grid.WorldMinCell =
        FIntPoint(int32(GridMin.X), int32(GridMin.Y));

    Grid.Width =
        int32(GridMax.X - GridMin.X);

    Grid.Height =
        int32(GridMax.Y - GridMin.Y);

    Grid.CellSize =
        Settings.HydrologyCellSize;

    const int32 Count =
        Grid.Width *
        Grid.Height;
    if (!VoxelGenerationAllocation::RetainArray(Grid.GroundPlane, Count, OutError) ||
        !VoxelGenerationAllocation::RetainArray(Grid.RainWeight, Count, OutError) ||
        !VoxelGenerationAllocation::RetainArray(Grid.Allowed, Count, OutError))
    {
        return false;
    }

    Grid.GroundPlane.SetNumUninitialized(
        Count);

    Grid.RainWeight.SetNumUninitialized(
        Count);

    Grid.Allowed.Init(
        1,
        Count);

    for (int32 LocalY = 0;
        LocalY < Grid.Height;
        ++LocalY)
    {
        if (InCancel &&
            InCancel->Load())
        {
            OutError = TEXT("Canceled");
            return false;
        }

        for (int32 LocalX = 0;
            LocalX < Grid.Width;
            ++LocalX)
        {
            const uint32 Index =
                Grid.ToIndex(
                    LocalX,
                    LocalY);

            const FIntPoint HydroCell = Grid.ToWorldHydrologyCell(Index);
            FIntPoint World;
            if (!Grid.TryToWorldVoxelXY(Index, World))
            {
                OutError = TEXT("Invalid hydrology grid cell outside world domain");
                return false;
            }

            Grid.GroundPlane[Index] =
                Terrain->SampleSurfaceHeight(World.X, World.Y);

            const int32 RainNoise =
                VoxelGeneration::Noise2D(
                    Settings.Seed,
                    HydroCell.X,
                    HydroCell.Y,
                    FMath::Max(
                        8,
                        Settings.ClimatePeriod /
                        FMath::Max(
                            1,
                            Settings.HydrologyCellSize)),
                    0x485944524F524149ull);

            /**
             * 1..8。
             * 不允许 0，否则大片区域不参与 accumulation。
             */
            Grid.RainWeight[Index] =
                static_cast<uint32>(
                    1 +
                    FMath::Clamp(
                        (RainNoise + 32768) *
                        7 /
                        65536,
                        0,
                        7));
        }
    }

    if (!AddOceanOutlets(Grid, OutError) || !AddCanonicalSinkOutlets(Grid, OutError))
    {
        return false;
    }

    if (Grid.OceanOutlets.IsEmpty() &&
        Grid.CanonicalSinkOutlets.IsEmpty())
    {
        OutError =
            TEXT(
                "Voxel hydrology grid has no "
                "certified outlet");

        return false;
    }

    OutGrid =
        MoveTemp(Grid);

    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::AddOceanOutlets(
    FVoxelHydrologyGrid& InOutGrid,
    FString& OutError) const
{
    const int32 SeaLevel =
        Recipe->Settings.SeaLevel;
    int32 OceanCount = 0;
    for (const int32 Ground : InOutGrid.GroundPlane)
    {
        OceanCount += Ground < SeaLevel ? 1 : 0;
    }
    if (!VoxelGenerationAllocation::RetainMap(InOutGrid.OceanOutlets, OceanCount, OutError))
    {
        return false;
    }

    for (uint32 Index = 0;
        Index <
            static_cast<uint32>(
                InOutGrid.GroundPlane.Num());
        ++Index)
    {
        if (InOutGrid.GroundPlane[Index] <
            SeaLevel)
        {
            InOutGrid.OceanOutlets.Add(
                Index,
                SeaLevel);
        }
    }
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::AddCanonicalSinkOutlets(
    FVoxelHydrologyGrid& InOutGrid,
    FString& OutError) const
{
    const int32 Spacing =
        Recipe->Settings.HydrologySinkSpacing;
	const int64 DomainMin = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InOutGrid.CellSize);
	const int64 DomainMax = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InOutGrid.CellSize) + 1;

    const FInt64Vector2 GridMaxExclusive(
        int64(InOutGrid.WorldMinCell.X) + InOutGrid.Width,
        int64(InOutGrid.WorldMinCell.Y) + InOutGrid.Height);

    const int64 MinSinkX =
        VoxelGeneration::FloorDivideWide(
            InOutGrid.WorldMinCell.X,
            Spacing);

    const int64 MinSinkY =
        VoxelGeneration::FloorDivideWide(
            InOutGrid.WorldMinCell.Y,
            Spacing);

    const int64 MaxSinkX =
        VoxelGeneration::FloorDivideWide(
            GridMaxExclusive.X - 1,
            Spacing);

    const int64 MaxSinkY =
        VoxelGeneration::FloorDivideWide(
            GridMaxExclusive.Y - 1,
            Spacing);
    int32 SinkCapacity = 0;

    for (int64 SinkY = MinSinkY;
        SinkY <= MaxSinkY;
        ++SinkY)
    {
        for (int64 SinkX = MinSinkX;
            SinkX <= MaxSinkX;
            ++SinkX)
        {
            const FInt64Vector2 SinkOrigin(SinkX * Spacing, SinkY * Spacing);
            const FInt64Vector2 SinkMin(FMath::Max(SinkOrigin.X, DomainMin), FMath::Max(SinkOrigin.Y, DomainMin));
            const FInt64Vector2 SinkMax(FMath::Min(SinkOrigin.X + Spacing, DomainMax),
                FMath::Min(SinkOrigin.Y + Spacing, DomainMax));

            /**
             * 只有完整物理 Sink Tile 被当前 Grid+Halo 包住
             * 才允许选 Canonical Sink。
             * 世界边缘只裁掉全局物理域外部分，所有 Region 使用同一裁切。
             *
             * 这样相邻 Region 不会因为缺少 Tile 一部分
             * 而选出不同的局部最低点。
             */
            if (SinkMin.X <
                    InOutGrid.WorldMinCell.X ||
                SinkMin.Y <
                    InOutGrid.WorldMinCell.Y ||
                SinkMax.X >
                    GridMaxExclusive.X ||
                SinkMax.Y >
                    GridMaxExclusive.Y)
            {
                continue;
            }

            int32 BestGround =
                MAX_int32;

            uint32 BestIndex =
                MAX_uint32;

            for (int64 WorldY = SinkMin.Y;
                WorldY < SinkMax.Y;
                ++WorldY)
            {
                for (int64 WorldX = SinkMin.X;
                    WorldX < SinkMax.X;
                    ++WorldX)
                {
                    const int32 LocalX = int32(WorldX - InOutGrid.WorldMinCell.X);

                    const int32 LocalY = int32(WorldY - InOutGrid.WorldMinCell.Y);

                    const uint32 Index =
                        InOutGrid.ToIndex(
                            LocalX,
                            LocalY);

                    const int32 Ground =
                        InOutGrid.GroundPlane[Index];

                    if (Ground <
                            BestGround ||
                        (Ground == BestGround &&
                         Index < BestIndex))
                    {
                        BestGround =
                            Ground;

                        BestIndex =
                            Index;
                    }
                }
            }

            if (BestIndex == MAX_uint32)
            {
                continue;
            }

            if (!InOutGrid.OceanOutlets.Contains(
                    BestIndex) &&
                !InOutGrid.CanonicalSinkOutlets.Contains(
                    BestIndex))
            {
                if (InOutGrid.CanonicalSinkOutlets.Num() == SinkCapacity)
                {
                    const int32 Capacity = NextHydrologyCapacity(SinkCapacity + 1, SinkCapacity);
                    if (!VoxelGenerationAllocation::RetainMap(InOutGrid.CanonicalSinkOutlets, Capacity, OutError))
                    {
                        return false;
                    }
                    SinkCapacity = Capacity;
                }
                InOutGrid.CanonicalSinkOutlets.Add(
                    BestIndex,
                    BestGround);
            }
        }
    }
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::BuildTopology(
    const FVoxelHydrologyRegionKey& InKey,
    FVoxelHydrologyPlan& OutPlan,
    FString& OutError,
    const TAtomic<bool>* InCancel) const
{
    const auto* Allocation = FVoxelGenerationAllocationScope::Get();
    if (!(Allocation ? Allocation->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
    {
        return false;
    }
    FVoxelHydrologyPlan Plan;
    Plan.Key = InKey;

    FInt64Vector2 CoreMin, CoreMax;
    if (!GetWorldHydrologyCore(InKey, Recipe->Settings, CoreMin, CoreMax))
    {
        OutError = TEXT("Invalid hydrology core outside world domain");
        return false;
    }
    const int64 CellSize = Recipe->Settings.HydrologyCellSize;
    Plan.CoreMin = FIntPoint(int32(FMath::Max(CoreMin.X * CellSize, VoxelGeneration::WorldCellMin)),
        int32(FMath::Max(CoreMin.Y * CellSize, VoxelGeneration::WorldCellMin)));
    Plan.CoreMax = FIntPoint(int32(FMath::Min(CoreMax.X * CellSize, VoxelGeneration::WorldCellMaxExclusive)),
        int32(FMath::Min(CoreMax.Y * CellSize, VoxelGeneration::WorldCellMaxExclusive)));

    if (!BuildHeightGrid(
            InKey,
            Plan.Grid,
            OutError,
            InCancel))
    {
        return false;
    }

    FVoxelGenerationWorkspace Workspace;
    FVoxelDrainageResult Drainage;
    ON_SCOPE_EXIT
    {
        VoxelGenerationAllocation::FreeRetainedArray(Drainage.Parent);
        VoxelGenerationAllocation::FreeRetainedArray(Drainage.SpillPlane);
        VoxelGenerationAllocation::FreeRetainedArray(Drainage.SettledOrder);
        VoxelGenerationAllocation::FreeRetainedArray(Drainage.Accumulation);
    };
    FVoxelDrainageInput DrainageInput;
    DrainageInput.WorldMinCell =
        Plan.Grid.WorldMinCell;
    DrainageInput.Width =
        Plan.Grid.Width;
    DrainageInput.Height =
        Plan.Grid.Height;
    if (!Workspace.ReserveArray(DrainageInput.GroundPlane, Plan.Grid.GroundPlane.Num(), OutError) ||
        !Workspace.ReserveArray(DrainageInput.RainWeight, Plan.Grid.RainWeight.Num(), OutError) ||
        !Workspace.ReserveArray(DrainageInput.Allowed, Plan.Grid.Allowed.Num(), OutError) ||
        !Workspace.ReserveMap(DrainageInput.Outlets,
            Plan.Grid.OceanOutlets.Num() + Plan.Grid.CanonicalSinkOutlets.Num(), OutError))
    {
        return false;
    }
    DrainageInput.GroundPlane.Append(Plan.Grid.GroundPlane);
    DrainageInput.RainWeight.Append(Plan.Grid.RainWeight);
    DrainageInput.Allowed.Append(Plan.Grid.Allowed);
    for (const auto& Pair : Plan.Grid.OceanOutlets)
    {
        DrainageInput.Outlets.Add(Pair.Key, Pair.Value);
    }

    for (const TPair<uint32, int32>& Pair :
        Plan.Grid.CanonicalSinkOutlets)
    {
        DrainageInput.Outlets.Add(
            Pair.Key,
            Pair.Value);
    }

    if (!VoxelHydrology::BuildDrainage(
            DrainageInput,
            Drainage,
            OutError,
            InCancel))
    {
        return false;
    }

    if (!BuildLakes(Plan.Grid, DrainageInput, Drainage, InKey, Plan.Lakes, OutError, InCancel))
    {
        return false;
    }
    Workspace.FreeArray(DrainageInput.GroundPlane);
    Workspace.FreeArray(DrainageInput.RainWeight);
    Workspace.FreeArray(DrainageInput.Allowed);
    Workspace.FreeMap(DrainageInput.Outlets);
    VoxelGenerationAllocation::FreeRetainedArray(Plan.Grid.Allowed);
    VoxelGenerationAllocation::FreeRetainedMap(Plan.Grid.CanonicalSinkOutlets);
    Plan.Lakes.Sort([](const FVoxelLakePlan& InFirst, const FVoxelLakePlan& InSecond)
    {
        return InFirst.Id < InSecond.Id;
    });
    if (!Plan.BuildSpatialIndex(OutError, InCancel)) return false;
    OutPlan = MoveTemp(Plan);
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::BuildPlan(
    const FVoxelHydrologyRegionKey& InKey,
    FVoxelHydrologyPlan& OutPlan,
    FString& OutError,
    const TAtomic<bool>* InCancel) const
{
    const auto* Allocation = FVoxelGenerationAllocationScope::Get();
    if (!(Allocation ? Allocation->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
    {
        return false;
    }
    FVoxelHydrologyPlan Plan;
    Plan.RiverShapeSmoothingPasses = Recipe->Settings.RiverShapeSmoothingPasses;
    Plan.RiverSeed = Recipe->Settings.Seed;
    Plan.RiverMeanderStrength = Recipe->Settings.RiverMeanderStrength;
    Plan.RiverMeanderFrequency = Recipe->Settings.RiverMeanderFrequency;
    Plan.RiverMeanderOctaves = Recipe->Settings.RiverMeanderOctaves;
    Plan.RiverMaxMeanderAngle = Recipe->Settings.RiverMaxMeanderAngle;
    Plan.Key = InKey;

    FInt64Vector2 CoreMin, CoreMax;
    if (!GetWorldHydrologyCore(InKey, Recipe->Settings, CoreMin, CoreMax))
    {
        OutError = TEXT("Invalid hydrology core outside world domain");
        return false;
    }
    const int64 CellSize = Recipe->Settings.HydrologyCellSize;
    Plan.CoreMin = FIntPoint(int32(FMath::Max(CoreMin.X * CellSize, VoxelGeneration::WorldCellMin)),
        int32(FMath::Max(CoreMin.Y * CellSize, VoxelGeneration::WorldCellMin)));
    Plan.CoreMax = FIntPoint(int32(FMath::Min(CoreMax.X * CellSize, VoxelGeneration::WorldCellMaxExclusive)),
        int32(FMath::Min(CoreMax.Y * CellSize, VoxelGeneration::WorldCellMaxExclusive)));

    if (!BuildHeightGrid(
            InKey,
            Plan.Grid,
            OutError,
            InCancel))
    {
        return false;
    }

    FVoxelGenerationWorkspace Workspace;
    FVoxelDrainageInput DrainageInput;
    DrainageInput.WorldMinCell =
        Plan.Grid.WorldMinCell;
    DrainageInput.Width =
        Plan.Grid.Width;
    DrainageInput.Height =
        Plan.Grid.Height;
    if (!Workspace.ReserveArray(DrainageInput.GroundPlane, Plan.Grid.GroundPlane.Num(), OutError) ||
        !Workspace.ReserveArray(DrainageInput.RainWeight, Plan.Grid.RainWeight.Num(), OutError) ||
        !Workspace.ReserveArray(DrainageInput.Allowed, Plan.Grid.Allowed.Num(), OutError) ||
        !Workspace.ReserveMap(DrainageInput.Outlets,
            Plan.Grid.OceanOutlets.Num() + Plan.Grid.CanonicalSinkOutlets.Num(), OutError))
    {
        return false;
    }
    DrainageInput.GroundPlane.Append(Plan.Grid.GroundPlane);
    DrainageInput.RainWeight.Append(Plan.Grid.RainWeight);
    DrainageInput.Allowed.Append(Plan.Grid.Allowed);
    for (const auto& Pair : Plan.Grid.OceanOutlets)
    {
        DrainageInput.Outlets.Add(Pair.Key, Pair.Value);
    }

    for (const TPair<uint32, int32>& Pair :
        Plan.Grid.CanonicalSinkOutlets)
    {
        DrainageInput.Outlets.Add(
            Pair.Key,
            Pair.Value);
    }

    if (!VoxelHydrology::BuildDrainage(
            DrainageInput,
            Plan.Drainage,
            OutError,
            InCancel))
    {
        return false;
    }

    TArray<FVoxelRiverSource> Sources;

    if (!SelectRiverSources(
        Plan.Grid,
        Plan.Drainage,
        InKey,
        Sources,
        OutError,
        Workspace))
    {
        return false;
    }

    TSet<uint32> ClaimedDownstream;
    int32 ClaimedCapacity = 0;

    for (const FVoxelRiverSource& Source :
        Sources)
    {
        if (InCancel &&
            InCancel->Load())
        {
            OutError = TEXT("Canceled");
            return false;
        }

        FVoxelRiverRoute Route;

        if (!TraceRiver(
                Plan.Grid,
                DrainageInput,
                Plan.Drainage,
                InKey,
                Source,
                ClaimedDownstream,
                ClaimedCapacity,
                Workspace,
                Route,
                OutError))
        {
            return false;
        }

        if (Route.Points.Num() >= 2)
        {
            if (Plan.Rivers.Num() == Plan.Rivers.Max() && !VoxelGenerationAllocation::RetainArray(
                Plan.Rivers, NextHydrologyCapacity(Plan.Rivers.Num() + 1, Plan.Rivers.Max()), OutError))
            {
                return false;
            }
            Plan.Rivers.Add(
                MoveTemp(Route));
        }
        else
        {
            VoxelGenerationAllocation::FreeRetainedArray(Route.Points);
        }
    }

    if (!BuildLakes(
            Plan.Grid,
            DrainageInput,
            Plan.Drainage,
            InKey,
            Plan.Lakes,
            OutError,
            InCancel))
    {
        return false;
    }
	Workspace.FreeArray(DrainageInput.GroundPlane);
	Workspace.FreeArray(DrainageInput.RainWeight);
	Workspace.FreeArray(DrainageInput.Allowed);
	Workspace.FreeMap(DrainageInput.Outlets);
	Workspace.FreeArray(Sources);
	Workspace.FreeSet(ClaimedDownstream);

    Plan.Rivers.Sort(
        [](const FVoxelRiverRoute& InA,
           const FVoxelRiverRoute& InB)
        {
            return InA.Id < InB.Id;
        });

    Plan.Lakes.Sort(
        [](const FVoxelLakePlan& InA,
           const FVoxelLakePlan& InB)
        {
            return InA.Id < InB.Id;
        });

	if (!Plan.Finalize(OutError, InCancel))
	{
		return false;
	}
	if (!Plan.ValidateRiverRoutes(OutError)) return false;

    OutPlan =
        MoveTemp(Plan);

    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::SelectRiverSources(
    const FVoxelHydrologyGrid& InGrid,
    const FVoxelDrainageResult& InDrainage,
    const FVoxelHydrologyRegionKey& InOwnerRegion,
    TArray<FVoxelRiverSource>& OutSources,
    FString& OutError,
    FVoxelGenerationWorkspace& InOutputWorkspace) const
{
    OutSources.Reset();

    const int32 SourceThreshold =
        Recipe->Settings.RiverSourceAccumulation;

    const int32 SourceSpacing =
        Recipe->Settings.RiverSourceSpacing;

    FInt64Vector2 CoreMin, CoreMax;
    if (!GetWorldHydrologyCore(InOwnerRegion, Recipe->Settings, CoreMin, CoreMax))
    {
        OutError = TEXT("Invalid hydrology source core outside world domain");
        return false;
    }

    FVoxelGenerationWorkspace Workspace;
    TMap<FIntPoint, FVoxelRiverSource>
        BestPerSourceCell;
    TArray<FVoxelRiverSource> Continuations;
    int32 SourceMapCapacity = 0;

    for (uint32 Index = 0;
        Index <
            static_cast<uint32>(
                InDrainage.Accumulation.Num());
        ++Index)
    {
        const uint64 Accumulation =
            InDrainage.Accumulation[Index];

        if (Accumulation <
            static_cast<uint64>(
                SourceThreshold))
        {
            continue;
        }

        const FIntPoint WorldCell =
            InGrid.ToWorldHydrologyCell(
                Index);

        if (WorldCell.X < CoreMin.X ||
            WorldCell.Y < CoreMin.Y ||
            WorldCell.X >= CoreMax.X ||
            WorldCell.Y >= CoreMax.Y)
        {
            continue;
        }

        /**
         * 只选“第一次达到阈值”的 Headwater。
         * 如果任意上游 Child 自己已经达到阈值，
         * 当前点属于已有河道下游，不再生成新 Source。
         */
        bool bHasQualifiedChild = false;
        bool bHasCoreQualifiedChild = false;
        bool bHasOutsideQualifiedChild = false;
		uint32 ContinuationUpstreamIndex = MAX_uint32;
		uint64 UpstreamAccumulation = 0;
		FIntPoint UpstreamWorldCell(MAX_int32, MAX_int32);

        for (uint8 Face = 0;
            Face < 4;
            ++Face)
        {
            uint32 Neighbor = 0;

            if (!TryGetNeighbor(
                    Index,
                    Face,
                    InGrid.Width,
                    InGrid.Height,
                    Neighbor))
            {
                continue;
            }

            if (InDrainage.Parent[Neighbor] ==
                    static_cast<int32>(Index) &&
                InDrainage.Accumulation[Neighbor] >=
                    static_cast<uint64>(
                        SourceThreshold))
            {
                bHasQualifiedChild = true;
                const FIntPoint Upstream = InGrid.ToWorldHydrologyCell(Neighbor);
                const bool bUpstreamInCore = Upstream.X >= CoreMin.X &&
                    Upstream.Y >= CoreMin.Y && Upstream.X < CoreMax.X &&
                    Upstream.Y < CoreMax.Y;
                bHasCoreQualifiedChild |= bUpstreamInCore;
                bHasOutsideQualifiedChild |= !bUpstreamInCore;
				if (!bUpstreamInCore &&
					(ContinuationUpstreamIndex == MAX_uint32 ||
					InDrainage.Accumulation[Neighbor] > UpstreamAccumulation ||
					(InDrainage.Accumulation[Neighbor] == UpstreamAccumulation &&
						(Upstream.X < UpstreamWorldCell.X ||
							(Upstream.X == UpstreamWorldCell.X && Upstream.Y < UpstreamWorldCell.Y)))))
				{
					ContinuationUpstreamIndex = Neighbor;
					UpstreamAccumulation = InDrainage.Accumulation[Neighbor];
					UpstreamWorldCell = Upstream;
				}
            }
        }

        const bool bContinuation = bHasQualifiedChild &&
            bHasOutsideQualifiedChild && !bHasCoreQualifiedChild;
        if (bHasQualifiedChild && !bContinuation)
        {
            continue;
        }

        const FIntPoint SourceBucket(
            VoxelGeneration::FloorDivide(
                WorldCell.X,
                SourceSpacing),
            VoxelGeneration::FloorDivide(
                WorldCell.Y,
                SourceSpacing));

        FVoxelRiverSource Candidate;
        Candidate.GridIndex = Index;
		Candidate.ContinuationUpstreamIndex = bContinuation
			? ContinuationUpstreamIndex : MAX_uint32;
        Candidate.Accumulation =
            Accumulation;
        Candidate.bContinuation = bContinuation;
        Candidate.Id =
            VoxelGeneration::MakeStableId(
                Recipe->Settings.Seed,
                FIntVector(
                    WorldCell.X,
                    WorldCell.Y,
                    0),
                bContinuation ? 0x5249564552434F4Eull : 0x5249564552535243ull,
                0);

        if (bContinuation)
        {
            if (Continuations.Num() == Continuations.Max() && !Workspace.ReserveArray(Continuations,
                NextHydrologyCapacity(Continuations.Num() + 1, Continuations.Max()), OutError))
            {
                return false;
            }
            Continuations.Add(Candidate);
            continue;
        }

        FVoxelRiverSource* Existing =
            BestPerSourceCell.Find(
                SourceBucket);

        if (!Existing ||
            Candidate.Accumulation >
                Existing->Accumulation ||
            (Candidate.Accumulation ==
                Existing->Accumulation &&
             Candidate.Id <
                Existing->Id))
        {
            if (!Existing && BestPerSourceCell.Num() == SourceMapCapacity)
            {
                const int32 Capacity = NextHydrologyCapacity(SourceMapCapacity + 1, SourceMapCapacity);
                if (!Workspace.ReserveMap(BestPerSourceCell, Capacity, OutError))
                {
                    return false;
                }
                SourceMapCapacity = Capacity;
            }
            BestPerSourceCell.FindOrAdd(SourceBucket) = Candidate;
        }
    }

    if (!InOutputWorkspace.ReserveArray(OutSources,
        BestPerSourceCell.Num() + Continuations.Num(), OutError))
    {
        return false;
    }
    for (const auto& Pair : BestPerSourceCell)
    {
        OutSources.Add(Pair.Value);
    }
    OutSources.Append(Continuations);

    OutSources.Sort(
        [](const FVoxelRiverSource& InA,
           const FVoxelRiverSource& InB)
        {
            return InA.Id < InB.Id;
        });
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::TraceRiver(
    const FVoxelHydrologyGrid& InGrid,
    const FVoxelDrainageInput& InInput,
    const FVoxelDrainageResult& InDrainage,
    const FVoxelHydrologyRegionKey& InOwnerRegion,
    const FVoxelRiverSource& InSource,
    TSet<uint32>& InOutClaimedDownstream,
    int32& InOutClaimedCapacity,
    FVoxelGenerationWorkspace& InWorkspace,
    FVoxelRiverRoute& OutRoute,
    FString& OutError) const
{
    FVoxelGenerationWorkspace Workspace;
    TArray<uint32> Path;

    if (!VoxelHydrology::TraceDrainage(
            InInput,
            InDrainage,
            InSource.GridIndex,
            Recipe->Settings.RiverTraceBudget,
            Path,
            OutError,
            &Workspace))
    {
        return false;
    }
	if (InSource.bContinuation &&
		InSource.ContinuationUpstreamIndex != MAX_uint32)
	{
		if (!Workspace.ReserveArray(Path, Path.Num() + 1, OutError))
		{
			return false;
		}
		Path.Insert(InSource.ContinuationUpstreamIndex, 0);
	}

    FVoxelRiverRoute Route;
    Route.Id =
        InSource.Id;
    Route.OwnerRegion =
        InOwnerRegion;
    Route.bContinuation = InSource.bContinuation;
    if (!VoxelGenerationAllocation::RetainArray(Route.Points, Path.Num(), OutError))
    {
        return false;
    }

    bool bHasBounds = false;

    for (int32 PathIndex = 0;
        PathIndex < Path.Num();
        ++PathIndex)
    {
        const uint32 GridIndex =
            Path[PathIndex];

        FIntPoint WorldVoxel;
        if (!InGrid.TryToWorldVoxelXY(GridIndex, WorldVoxel))
        {
            OutError = TEXT("Invalid river route cell outside world domain");
            return false;
        }

        const uint64 Accumulation =
            InDrainage.Accumulation[
                GridIndex];

        const FResolvedRiverSize Size = ResolveRiverSize(
            Accumulation, Recipe->Settings);

        FVoxelRiverRoutePoint Point;
        Point.Position =
            WorldVoxel;

        Point.WaterZ =
            InDrainage.SpillPlane[
                GridIndex];

        Point.BedHalfWidth = Size.BedHalfWidth;
        Point.WaterHalfWidth = Size.WaterHalfWidth;
        Point.BankWidth = Size.BankWidth;
        Point.ShoreWidth = Size.ShoreWidth;
        Point.Depth = Size.Depth;

        Point.Accumulation =
            Accumulation;

        Route.Points.Add(
            Point);

        if (!bHasBounds)
        {
            Route.Min =
                WorldVoxel;
            Route.Max =
                WorldVoxel;
            bHasBounds = true;
        }
        else
        {
            Route.Min.X =
                FMath::Min(
                    Route.Min.X,
                    WorldVoxel.X);

            Route.Min.Y =
                FMath::Min(
                    Route.Min.Y,
                    WorldVoxel.Y);

            Route.Max.X =
                FMath::Max(
                    Route.Max.X,
                    WorldVoxel.X);

            Route.Max.Y =
                FMath::Max(
                    Route.Max.Y,
                    WorldVoxel.Y);
        }

        /**
         * 已经有更早 StableId Route 声明该下游 Cell，
         * 当前 Route 到这里结束并视为汇流。
         *
         * Source 自己的第一个 Cell 不能触发。
         */
        if (PathIndex > 0 &&
            InOutClaimedDownstream.Contains(
                GridIndex))
        {
            break;
        }

        if (!InOutClaimedDownstream.Contains(GridIndex))
        {
            if (InOutClaimedDownstream.Num() == InOutClaimedCapacity)
            {
                const int32 Capacity = NextHydrologyCapacity(InOutClaimedCapacity + 1, InOutClaimedCapacity);
                if (!InWorkspace.ReserveSet(InOutClaimedDownstream, Capacity, OutError))
                {
                    return false;
                }
                InOutClaimedCapacity = Capacity;
            }
            InOutClaimedDownstream.Add(GridIndex);
        }
    }

    OutRoute =
        MoveTemp(Route);

    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGenerator::BuildLakes(
    const FVoxelHydrologyGrid& InGrid,
    const FVoxelDrainageInput& InInput,
    const FVoxelDrainageResult& InDrainage,
    const FVoxelHydrologyRegionKey& InOwnerRegion,
    TArray<FVoxelLakePlan>& OutLakes,
    FString& OutError,
    const TAtomic<bool>* InCancel) const
{
    OutLakes.Reset();

    const int32 Count =
        InGrid.GroundPlane.Num();

    FVoxelGenerationWorkspace Workspace;
    TArray<int32> Root;
    if (!Workspace.ReserveArray(Root, Count, OutError))
    {
        return false;
    }
    Root.Init(
        INDEX_NONE,
        Count);

    auto ResolveRoot =
        [&InDrainage, &Root, Count](
            int32 InStart) -> int32
        {
            int32 Current =
                InStart;

            while (Current >= 0 &&
                Current < Count)
            {
                if (Root[Current] !=
                    INDEX_NONE)
                {
                    Current =
                        Root[Current];
                    break;
                }

                const int32 Parent =
                    InDrainage.Parent[Current];

                if (Parent == -1)
                {
                    break;
                }

                Current =
                    Parent;
            }

            const int32 Resolved =
                Current;

            Current = InStart;
            while (Current >= 0 && Current < Count && Root[Current] == INDEX_NONE)
            {
                const int32 Parent = InDrainage.Parent[Current];
                Root[Current] = Resolved;
                if (Parent == -1)
                {
                    break;
                }
                Current = Parent;
            }

            return Resolved;
        };

    for (int32 Index = 0;
        Index < Count;
        ++Index)
    {
        if (InCancel &&
            InCancel->Load())
        {
            OutError = TEXT("Canceled");
            return false;
        }

        ResolveRoot(Index);
    }

    TMap<int32, int32> MinimumSaddle;
    int32 SaddleCapacity = 0;

    for (int32 Index = 0;
        Index < Count;
        ++Index)
    {
        for (uint8 Face = 0;
            Face < 4;
            ++Face)
        {
            uint32 Neighbor = 0;

            if (!TryGetNeighbor(
                    static_cast<uint32>(Index),
                    Face,
                    InGrid.Width,
                    InGrid.Height,
                    Neighbor))
            {
                continue;
            }

            const int32 RootA =
                Root[Index];

            const int32 RootB =
                Root[Neighbor];

            if (RootA == RootB)
            {
                continue;
            }

            const int32 Saddle =
                FMath::Max(
                    InGrid.GroundPlane[Index],
                    InGrid.GroundPlane[Neighbor]);

            int32* Existing =
                MinimumSaddle.Find(
                    RootA);

            if (!Existing ||
                Saddle < *Existing)
            {
                if (!Existing && MinimumSaddle.Num() == SaddleCapacity)
                {
                    const int32 Capacity = NextHydrologyCapacity(SaddleCapacity + 1, SaddleCapacity);
                    if (!Workspace.ReserveMap(MinimumSaddle, Capacity, OutError))
                    {
                        return false;
                    }
                    SaddleCapacity = Capacity;
                }
                MinimumSaddle.FindOrAdd(RootA) = Saddle;
            }
        }
    }

    TArray<uint32> SinkIndices;
    if (!Workspace.ReserveArray(SinkIndices, InGrid.CanonicalSinkOutlets.Num(), OutError))
    {
        return false;
    }
    for (const auto& Pair : InGrid.CanonicalSinkOutlets)
    {
        SinkIndices.Add(Pair.Key);
    }

    SinkIndices.Sort();

    for (uint32 SinkIndex :
        SinkIndices)
    {
        if (InGrid.OceanOutlets.Contains(
                SinkIndex))
        {
            continue;
        }

        const int32 SinkRoot =
            Root[SinkIndex];

        if (SinkRoot !=
            static_cast<int32>(SinkIndex))
        {
            /**
             * 该 Canonical Sink 被更低/海洋 Outlet 捕获。
             */
            continue;
        }

        const int32* Saddle =
            MinimumSaddle.Find(
                SinkRoot);

        if (!Saddle)
        {
            continue;
        }

        const int32 WaterPlane =
            *Saddle;

        if (WaterPlane <=
            InGrid.GroundPlane[SinkIndex])
        {
            continue;
        }

        FVoxelGenerationWorkspace LakeWorkspace;
        TArray<uint8> WaterMask;
        if (!LakeWorkspace.ReserveArray(WaterMask, Count, OutError))
        {
            return false;
        }
        WaterMask.Init(
            0,
            Count);

        int32 WaterCellCount = 0;

        for (int32 Index = 0;
            Index < Count;
            ++Index)
        {
            if (Root[Index] ==
                    SinkRoot &&
                InGrid.GroundPlane[Index] <
                    WaterPlane)
            {
                WaterMask[Index] = 1;
                ++WaterCellCount;
            }
        }

        if (WaterCellCount <= 0 ||
            WaterCellCount >
                Recipe->Settings.LakeMaxCells)
        {
            continue;
        }

        const FIntPoint SinkWorldCell =
            InGrid.ToWorldHydrologyCell(
                SinkIndex);

        const FVoxelHydrologyRegionKey
            StableOwner
            {
                FIntPoint(
                    VoxelGeneration::FloorDivide(
                        SinkWorldCell.X,
                        Recipe->Settings.
                            HydrologyRegionSide),
                    VoxelGeneration::FloorDivide(
                        SinkWorldCell.Y,
                        Recipe->Settings.
                            HydrologyRegionSide))
            };

        if (!(StableOwner ==
            InOwnerRegion))
        {
            continue;
        }

        FVoxelBasinInput BasinInput;
        BasinInput.Width =
            InGrid.Width;
        BasinInput.Height =
            InGrid.Height;
        BasinInput.WaterPlane =
            WaterPlane;
        if (!LakeWorkspace.ReserveArray(BasinInput.GroundPlane, Count, OutError) ||
            !LakeWorkspace.ReserveArray(BasinInput.Known, Count, OutError) ||
            !LakeWorkspace.ReserveArray(BasinInput.WaterMask, Count, OutError))
        {
            return false;
        }
        BasinInput.GroundPlane.Append(InGrid.GroundPlane);
        BasinInput.Known.Init(
            1,
            Count);
        BasinInput.WaterMask.Append(WaterMask);

        /**
         * 当前 Grid+Halo 已经完整包含候选 footprint。
         * 这里只认证 Grid 内完整 Shore。
         * 如果未来允许跨 Hydrology Plan Lake，
         * 必须由上层先提供已经认证的 ConnectorPlanes；
         * 不能在这里把 Grid 边缘当 Connector。
         */

        FVoxelBasinCertificate Certificate;

        FString BasinError;

        if (!VoxelHydrology::ValidateBasin(
                BasinInput,
                Certificate,
                BasinError,
                InCancel))
        {
            const auto* Allocation = FVoxelGenerationAllocationScope::Get();
            if (Allocation && !Allocation->Check(OutError))
            {
                return false;
            }
            if (BasinError == TEXT("Canceled") || BasinError.StartsWith(TEXT("InvalidAllocationScope")) ||
                BasinError.StartsWith(TEXT("CapacityBlocked")) || BasinError.StartsWith(TEXT("Oversized")))
            {
                OutError = MoveTemp(BasinError);
                return false;
            }
            /**
             * 候选 Lake 不成立不是整个 Hydrology Plan 失败；
             * 它可能只是开放谷地/河谷。
             */
            continue;
        }

        FVoxelLakePlan Lake;
        Lake.OwnerRegion =
            StableOwner;
        Lake.WaterZ =
            WaterPlane;
        Lake.Certificate =
            Certificate;

        Lake.Id =
            VoxelGeneration::MakeStableId(
                Recipe->Settings.Seed,
                FIntVector(
                    SinkWorldCell.X,
                    SinkWorldCell.Y,
                    WaterPlane),
                0x4C414B45504C414Eull,
                0);

        if (!VoxelGenerationAllocation::RetainArray(Lake.Cells, WaterCellCount, OutError))
        {
            return false;
        }

        for (uint32 Index = 0;
            Index <
                static_cast<uint32>(Count);
            ++Index)
        {
            if (WaterMask[Index] != 0)
            {
                FIntPoint WorldPoint;
                if (!InGrid.TryToWorldVoxelXY(Index, WorldPoint))
                {
                    OutError = TEXT("Invalid lake cell outside world domain");
                    return false;
                }
                Lake.Cells.Add(WorldPoint);
            }
        }

        if (OutLakes.Num() == OutLakes.Max() && !VoxelGenerationAllocation::RetainArray(
            OutLakes, NextHydrologyCapacity(OutLakes.Num() + 1, OutLakes.Max()), OutError))
        {
            return false;
        }
        OutLakes.Add(
            MoveTemp(Lake));
    }

    OutError.Reset();
    return true;
}
