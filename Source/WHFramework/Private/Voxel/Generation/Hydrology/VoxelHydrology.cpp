#include "Voxel/Generation/Hydrology/VoxelHydrology.h"

#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

namespace
{
    constexpr int32 HydrologyMinPlane = -32768;
    constexpr int32 HydrologyMaxPlane = 32768;

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
    };

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

        return InA.Index <
            InB.Index;
    }

    void HydrologyHeapPush(
        TArray<FHydrologyHeapEntry>& InOutHeap,
        const FHydrologyHeapEntry& InEntry)
    {
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

    Result.SpillPlane.Init(
        MAX_int32,
        Count);

    Result.Parent.Init(
        -2,
        Count);

    Result.Accumulation.Init(
        0,
        Count);

    Result.SettledOrder.Reserve(
        Count);

    TArray<uint8> Settled;
    Settled.Init(
        0,
        Count);

    TArray<uint8> Terminal;
    Terminal.Init(
        0,
        Count);

    TArray<FHydrologyHeapEntry> Heap;
    Heap.Reserve(
        InInput.Outlets.Num());

    TArray<uint32> SortedOutletIndices;
    InInput.Outlets.GetKeys(
        SortedOutletIndices);

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

        HydrologyHeapPush(
            Heap,
            {
                Result.SpillPlane[Index],
                Index
            });
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

                HydrologyHeapPush(
                    Heap,
                    {
                        Candidate,
                        Neighbor
                    });
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
    FString& OutError)
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
    Path.Reserve(
        FMath::Min(
            InRouteBudget,
            Count));

    TArray<uint8> Seen;
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

    OutPath =
        MoveTemp(Path);

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

    TArray<uint8> Seen;
    Seen.Init(
        0,
        Count);

    TArray<uint32> Queue;
    Queue.Reserve(
        Certificate.WaterCells);

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
    if (InOriginalGround <
            HydrologyMinPlane ||
        InOriginalGround >
            HydrologyMaxPlane ||
        InWaterPlane <
            HydrologyMinPlane ||
        InWaterPlane >
            HydrologyMaxPlane - 1 ||
        InDistanceCells < 0 ||
        InDistanceCells > 65536 ||
        InShape.HalfWidth < 1 ||
        InShape.HalfWidth > 256 ||
        InShape.BankWidth < 1 ||
        InShape.BankWidth > 512 ||
        InShape.ShoreWidth < 1 ||
        InShape.ShoreWidth > 1024 ||
        InShape.Depth < 1 ||
        InShape.Depth > 256 ||
        InShape.MaxCutFill < 1 ||
        InShape.MaxCutFill > 512 ||
        InWaterPlane -
            InShape.Depth <
            HydrologyMinPlane)
    {
        OutError =
            TEXT("Invalid river section");

        return false;
    }

    FVoxelRiverSection Section;

    Section.WaterPlane =
        InWaterPlane;

    Section.BedPlane =
        InWaterPlane -
        InShape.Depth;

    if (InDistanceCells <=
        InShape.HalfWidth)
    {
        Section.GroundPlane =
            Section.BedPlane;
    }
    else if (InDistanceCells <=
        InShape.HalfWidth +
        InShape.BankWidth)
    {
        const int32 AlphaQ16 =
            static_cast<int32>(
                static_cast<int64>(
                    InDistanceCells -
                    InShape.HalfWidth) *
                65536 /
                InShape.BankWidth);

        Section.GroundPlane =
            Section.BedPlane +
            static_cast<int32>(
                static_cast<int64>(
                    InShape.Depth + 1) *
                SmoothQ16(AlphaQ16) /
                65536);
    }
    else if (InDistanceCells <
        InShape.HalfWidth +
        InShape.BankWidth +
        InShape.ShoreWidth)
    {
        const int32 AlphaQ16 =
            static_cast<int32>(
                static_cast<int64>(
                    InDistanceCells -
                    InShape.HalfWidth -
                    InShape.BankWidth) *
                65536 /
                InShape.ShoreWidth);

        Section.GroundPlane =
            InWaterPlane +
            1 +
            static_cast<int32>(
                (static_cast<int64>(
                    InOriginalGround) -
                    InWaterPlane -
                    1) *
                SmoothQ16(AlphaQ16) /
                65536);
    }
    else
    {
        Section.GroundPlane =
            InOriginalGround;
    }

    if (FMath::Abs(
        static_cast<int64>(
            Section.GroundPlane) -
        static_cast<int64>(
            InOriginalGround)) >
        InShape.MaxCutFill)
    {
        OutError =
            TEXT(
                "River route exceeds "
                "cut/fill budget");

        return false;
    }

    Section.bWet =
        InDistanceCells <=
            InShape.HalfWidth +
            InShape.BankWidth &&
        Section.GroundPlane <
            InWaterPlane;

    OutSection =
        Section;

    OutError.Reset();
    return true;
}

bool FVoxelHydrologyGrid::IsValidIndex(
    uint32 InIndex) const
{
    return InIndex <
        static_cast<uint32>(
            Width * Height);
}

uint32 FVoxelHydrologyGrid::ToIndex(
    int32 InX,
    int32 InY) const
{
    check(InX >= 0);
    check(InY >= 0);
    check(InX < Width);
    check(InY < Height);

    return static_cast<uint32>(
        InX +
        InY * Width);
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

FIntPoint FVoxelHydrologyGrid::ToWorldVoxelXY(
    uint32 InIndex) const
{
    const FIntPoint HydrologyCell =
        ToWorldHydrologyCell(InIndex);

    return FIntPoint(
        HydrologyCell.X * CellSize,
        HydrologyCell.Y * CellSize);
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

    const FIntPoint CoreMin(
        InKey.Coordinate.X * RegionSide,
        InKey.Coordinate.Y * RegionSide);

    const FIntPoint GridMin =
        CoreMin -
        FIntPoint(Halo, Halo);

    const int32 GridSide =
        RegionSide +
        Halo * 2;

    if (GridSide <= 0 ||
        GridSide > 4096)
    {
        OutError =
            TEXT("Voxel hydrology grid side is invalid");

        return false;
    }

    FVoxelHydrologyGrid Grid;

    Grid.WorldMinCell =
        GridMin;

    Grid.Width =
        GridSide;

    Grid.Height =
        GridSide;

    Grid.CellSize =
        Settings.HydrologyCellSize;

    const int32 Count =
        Grid.Width *
        Grid.Height;

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

            const FIntPoint HydroCell =
                Grid.WorldMinCell +
                FIntPoint(
                    LocalX,
                    LocalY);

            const int32 WorldX =
                HydroCell.X *
                Grid.CellSize;

            const int32 WorldY =
                HydroCell.Y *
                Grid.CellSize;

            const FVoxelMacroTerrainSample TerrainSample =
                Terrain->SampleMacro(
                    WorldX,
                    WorldY);

            Grid.GroundPlane[Index] =
                TerrainSample.SurfaceZ;

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

    AddOceanOutlets(Grid);
    AddCanonicalSinkOutlets(Grid);

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

void FVoxelHydrologyGenerator::AddOceanOutlets(
    FVoxelHydrologyGrid& InOutGrid) const
{
    const int32 SeaLevel =
        Recipe->Settings.SeaLevel;

    for (uint32 Index = 0;
        Index <
            static_cast<uint32>(
                InOutGrid.GroundPlane.Num());
        ++Index)
    {
        if (InOutGrid.GroundPlane[Index] <=
            SeaLevel)
        {
            InOutGrid.OceanOutlets.Add(
                Index,
                SeaLevel);
        }
    }
}

void FVoxelHydrologyGenerator::AddCanonicalSinkOutlets(
    FVoxelHydrologyGrid& InOutGrid) const
{
    const int32 Spacing =
        Recipe->Settings.HydrologySinkSpacing;

    const FIntPoint GridMaxExclusive =
        InOutGrid.WorldMinCell +
        FIntPoint(
            InOutGrid.Width,
            InOutGrid.Height);

    const int32 MinSinkX =
        VoxelGeneration::FloorDivide(
            InOutGrid.WorldMinCell.X,
            Spacing);

    const int32 MinSinkY =
        VoxelGeneration::FloorDivide(
            InOutGrid.WorldMinCell.Y,
            Spacing);

    const int32 MaxSinkX =
        VoxelGeneration::FloorDivide(
            GridMaxExclusive.X - 1,
            Spacing);

    const int32 MaxSinkY =
        VoxelGeneration::FloorDivide(
            GridMaxExclusive.Y - 1,
            Spacing);

    for (int32 SinkY = MinSinkY;
        SinkY <= MaxSinkY;
        ++SinkY)
    {
        for (int32 SinkX = MinSinkX;
            SinkX <= MaxSinkX;
            ++SinkX)
        {
            const FIntPoint SinkMin(
                SinkX * Spacing,
                SinkY * Spacing);

            const FIntPoint SinkMax =
                SinkMin +
                FIntPoint(
                    Spacing,
                    Spacing);

            /**
             * 只有完整 Sink Tile 被当前 Grid+Halo 包住
             * 才允许选 Canonical Sink。
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

            for (int32 WorldY = SinkMin.Y;
                WorldY < SinkMax.Y;
                ++WorldY)
            {
                for (int32 WorldX = SinkMin.X;
                    WorldX < SinkMax.X;
                    ++WorldX)
                {
                    const int32 LocalX =
                        WorldX -
                        InOutGrid.WorldMinCell.X;

                    const int32 LocalY =
                        WorldY -
                        InOutGrid.WorldMinCell.Y;

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
                InOutGrid.CanonicalSinkOutlets.Add(
                    BestIndex,
                    BestGround);
            }
        }
    }
}

bool FVoxelHydrologyGenerator::BuildPlan(
    const FVoxelHydrologyRegionKey& InKey,
    FVoxelHydrologyPlan& OutPlan,
    FString& OutError,
    const TAtomic<bool>* InCancel) const
{
    FVoxelHydrologyPlan Plan;
    Plan.Key = InKey;

    const int32 RegionSide =
        Recipe->Settings.HydrologyRegionSide;

    Plan.CoreMin =
        FIntPoint(
            InKey.Coordinate.X *
                RegionSide,
            InKey.Coordinate.Y *
                RegionSide);

    Plan.CoreMax =
        Plan.CoreMin +
        FIntPoint(
            RegionSide,
            RegionSide);

    if (!BuildHeightGrid(
            InKey,
            Plan.Grid,
            OutError,
            InCancel))
    {
        return false;
    }

    FVoxelDrainageInput DrainageInput;
    DrainageInput.Width =
        Plan.Grid.Width;
    DrainageInput.Height =
        Plan.Grid.Height;
    DrainageInput.GroundPlane =
        Plan.Grid.GroundPlane;
    DrainageInput.RainWeight =
        Plan.Grid.RainWeight;
    DrainageInput.Allowed =
        Plan.Grid.Allowed;
    DrainageInput.Outlets =
        Plan.Grid.OceanOutlets;

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

    SelectRiverSources(
        Plan.Grid,
        Plan.Drainage,
        InKey,
        Sources);

    TSet<uint32> ClaimedDownstream;

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
                Route,
                OutError))
        {
            return false;
        }

        if (Route.Points.Num() >= 2)
        {
            Plan.Rivers.Add(
                MoveTemp(Route));
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

	Plan.Finalize();

    OutPlan =
        MoveTemp(Plan);

    OutError.Reset();
    return true;
}

void FVoxelHydrologyGenerator::SelectRiverSources(
    const FVoxelHydrologyGrid& InGrid,
    const FVoxelDrainageResult& InDrainage,
    const FVoxelHydrologyRegionKey& InOwnerRegion,
    TArray<FVoxelRiverSource>& OutSources) const
{
    OutSources.Reset();

    const int32 RegionSide =
        Recipe->Settings.HydrologyRegionSide;

    const int32 SourceThreshold =
        Recipe->Settings.RiverSourceAccumulation;

    const int32 SourceSpacing =
        Recipe->Settings.RiverSourceSpacing;

    const FIntPoint CoreMin(
        InOwnerRegion.Coordinate.X *
            RegionSide,
        InOwnerRegion.Coordinate.Y *
            RegionSide);

    const FIntPoint CoreMax =
        CoreMin +
        FIntPoint(
            RegionSide,
            RegionSide);

    TMap<FIntPoint, FVoxelRiverSource>
        BestPerSourceCell;

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
                break;
            }
        }

        if (bHasQualifiedChild)
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
        Candidate.Accumulation =
            Accumulation;
        Candidate.Id =
            VoxelGeneration::MakeStableId(
                Recipe->Settings.Seed,
                FIntVector(
                    WorldCell.X,
                    WorldCell.Y,
                    0),
                0x5249564552535243ull,
                0);

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
            BestPerSourceCell.Add(
                SourceBucket,
                Candidate);
        }
    }

    BestPerSourceCell.GenerateValueArray(
        OutSources);

    OutSources.Sort(
        [](const FVoxelRiverSource& InA,
           const FVoxelRiverSource& InB)
        {
            return InA.Id < InB.Id;
        });
}

bool FVoxelHydrologyGenerator::TraceRiver(
    const FVoxelHydrologyGrid& InGrid,
    const FVoxelDrainageInput& InInput,
    const FVoxelDrainageResult& InDrainage,
    const FVoxelHydrologyRegionKey& InOwnerRegion,
    const FVoxelRiverSource& InSource,
    TSet<uint32>& InOutClaimedDownstream,
    FVoxelRiverRoute& OutRoute,
    FString& OutError) const
{
    TArray<uint32> Path;

    if (!VoxelHydrology::TraceDrainage(
            InInput,
            InDrainage,
            InSource.GridIndex,
            Recipe->Settings.RiverTraceBudget,
            Path,
            OutError))
    {
        return false;
    }

    FVoxelRiverRoute Route;
    Route.Id =
        InSource.Id;
    Route.OwnerRegion =
        InOwnerRegion;

    bool bHasBounds = false;

    for (int32 PathIndex = 0;
        PathIndex < Path.Num();
        ++PathIndex)
    {
        const uint32 GridIndex =
            Path[PathIndex];

        const FIntPoint WorldVoxel =
            InGrid.ToWorldVoxelXY(
                GridIndex);

        const uint64 Accumulation =
            InDrainage.Accumulation[
                GridIndex];

        const int32 WidthScale =
            FMath::Clamp(
                static_cast<int32>(
                    FMath::FloorLog2(
                        static_cast<uint32>(
                            FMath::Clamp<uint64>(
                                Accumulation,
                                1,
                                MAX_uint32)))),
                0,
                8);

        FVoxelRiverRoutePoint Point;
        Point.Position =
            WorldVoxel;

        Point.WaterZ =
            InDrainage.SpillPlane[
                GridIndex];

        Point.HalfWidth =
            Recipe->Settings.RiverBaseHalfWidth +
            WidthScale;

        Point.Depth =
            Recipe->Settings.RiverBaseDepth +
            WidthScale / 2;

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

        InOutClaimedDownstream.Add(
            GridIndex);
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

    TArray<int32> Root;
    Root.Init(
        INDEX_NONE,
        Count);

    auto ResolveRoot =
        [&InDrainage, &Root, Count](
            int32 InStart) -> int32
        {
            int32 Current =
                InStart;

            TArray<int32, TInlineAllocator<64>>
                Chain;

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

                Chain.Add(
                    Current);

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

            for (int32 Cell :
                Chain)
            {
                Root[Cell] =
                    Resolved;
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
                MinimumSaddle.Add(
                    RootA,
                    Saddle);
            }
        }
    }

    TArray<uint32> SinkIndices;
    InGrid.CanonicalSinkOutlets.GetKeys(
        SinkIndices);

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

        TArray<uint8> WaterMask;
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
        BasinInput.GroundPlane =
            InGrid.GroundPlane;
        BasinInput.Known.Init(
            1,
            Count);
        BasinInput.WaterMask =
            WaterMask;

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

        Lake.Cells.Reserve(
            WaterCellCount);

        for (uint32 Index = 0;
            Index <
                static_cast<uint32>(Count);
            ++Index)
        {
            if (WaterMask[Index] != 0)
            {
                Lake.Cells.Add(
                    InGrid.ToWorldVoxelXY(
                        Index));
            }
        }

        OutLakes.Add(
            MoveTemp(Lake));
    }

    OutError.Reset();
    return true;
}
