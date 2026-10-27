#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

namespace
{
    bool HydrologyComponentCellLess(const FIntPoint& InFirst, const FIntPoint& InSecond)
    {
        return InFirst.X < InSecond.X || (InFirst.X == InSecond.X && InFirst.Y < InSecond.Y);
    }
}

FVoxelHydrologyComponentBuild::FVoxelHydrologyComponentBuild(
    TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> InConfig,
    FVoxelGenerationPlanCache& InCache, const FIntPoint& InSeedCell)
    : Config(InConfig), Recipe(InConfig->Recipe.ToSharedRef()), Cache(InCache), SeedCell(InSeedCell)
{
    const int32 Side = Recipe->Settings.HydrologyRegionSide;
    Region.Coordinate = FIntPoint(VoxelGeneration::FloorDivide(SeedCell.X, Side), VoxelGeneration::FloorDivide(SeedCell.Y, Side));
    Generation = Cache.GetHydrologyGeneration();
    Result.Generation = Generation;
}

FVoxelHydrologyComponentBuild::~FVoxelHydrologyComponentBuild()
{
}

bool FVoxelHydrologyComponentBuild::AddMissingTile(const FVoxelHydrologyRegionKey& InKey, FString& OutError)
{
    if (MissingTiles.Contains(InKey)) return true;
    if (!VoxelGenerationAllocation::RetainArray(MissingTiles, MissingTiles.Num() + 1, OutError)) return false;
    MissingTiles.Add(InKey);
    return true;
}

void FVoxelHydrologyComponentBuild::VisitSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
    FVoxelGenerationRequiredSourceScope::VisitLease(Config->InputLease.Get(), InVisit);
    FVoxelGenerationRequiredSourceScope::VisitLease(Config->RecipeInputLease.Get(), InVisit);
    if (Graph) FVoxelGenerationRequiredSourceScope::VisitLease(Graph->ResourceLease.Get(), InVisit);
    if (Runoff) FVoxelGenerationRequiredSourceScope::VisitLease(Runoff->ResourceLease.Get(), InVisit);
    if (Sources) FVoxelGenerationRequiredSourceScope::VisitLease(Sources->ResourceLease.Get(), InVisit);
    if (Channels) FVoxelGenerationRequiredSourceScope::VisitLease(Channels->ResourceLease.Get(), InVisit);
}

EVoxelHydrologyComponentStep FVoxelHydrologyComponentBuild::Advance(FTileLookup InLookup,
    int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
    OutError.Reset();
    MissingTiles.Reset();
    TOptional<FVoxelHydrologyRegionKey> RequestedTile;
    const auto Request = [&](const FVoxelHydrologyRegionKey& InKey, FString&)
    {
        if (!RequestedTile.IsSet()) RequestedTile = InKey;
        return true;
    };
    const auto Failure = [&]()
    {
        if (OutError == TEXT("Canceled")) return EVoxelHydrologyComponentStep::Canceled;
        if (RequestedTile.IsSet() && !AddMissingTile(RequestedTile.GetValue(), OutError))
            return OutError.StartsWith(TEXT("CapacityBlocked")) ? EVoxelHydrologyComponentStep::CapacityBlocked : EVoxelHydrologyComponentStep::Failed;
        if (!MissingTiles.IsEmpty()) return EVoxelHydrologyComponentStep::NeedTiles;
        if (OutError.StartsWith(TEXT("Yielded")) || OutError.StartsWith(TEXT("DependencyNotReady"))) return EVoxelHydrologyComponentStep::Yielded;
        if (OutError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyComponentStep::CapacityBlocked;
        return EVoxelHydrologyComponentStep::Failed;
    };
    if ((InCancel && InCancel->Load()) || Generation != Cache.GetHydrologyGeneration())
    {
        OutError = TEXT("Canceled");
        return EVoxelHydrologyComponentStep::Canceled;
    }
    if (bReady) return EVoxelHydrologyComponentStep::Ready;
    if (InWorkBudget <= 0) return EVoxelHydrologyComponentStep::Yielded;
    if (!bLocalReady)
    {
        if (!Graph && !Cache.AdvanceHydrologyGraph(Region, Config, InLookup, Request, Graph, OutError, InCancel)) return Failure();
        if (!Runoff && !Cache.AdvanceHydrologyRunoff(Region, Config, InLookup, Request, Runoff, OutError, InCancel)) return Failure();
        if (!Sources && !Cache.AdvanceHydrologySources(Region, Config, InLookup, Request, Sources, OutError, InCancel)) return Failure();
        if (!Channels && !Cache.AdvanceHydrologyChannels(Region, Config, InLookup, Request, Channels, OutError, InCancel)) return Failure();
        if (!LocalBuild.IsSet())
        {
            LocalBuild.Emplace();
            if (!LocalBuild->Initialize(*Graph, OutError)) return Failure();
        }
        const auto ReadCell = [&](const FIntPoint& InCell, FVoxelHydrologyRegionSample& OutSample, FString& InError)
        {
            FIntPoint World;
            return FVoxelHydrologyMacroInput::Read(Recipe->Settings, InCell, InLookup, Request, OutSample, World, InError);
        };
        const auto Step = LocalBuild->Advance(*Graph, *Runoff, ReadCell, InWorkBudget, OutError, InCancel);
        if (Step != EVoxelHydrologyRegionStep::Ready)
        {
            if (Step == EVoxelHydrologyRegionStep::Yielded && OutError.IsEmpty()) OutError = TEXT("Yielded: local macro runoff");
            return Failure();
        }
        Local = LocalBuild->TakeResult();
        LocalBuild.Reset();
        Result.TotalRainWeight = Local.LocalRainWeight;
        Result.IncomingRainWeight = Local.IncomingRainWeight;
        Result.ReceivedRainWeight = Local.ReceivedRainWeight;
        Result.ExportedRainWeight = Local.ExportedRainWeight;
        Result.Key = Local.Minimum;
        Graph.Reset();
        Runoff.Reset();
        bLocalReady = true;
    }
    if (!bNodesFiltered)
    {
        if (!PrepareRiverNodes(InLookup, Request, OutError, InCancel)) return Failure();
        bNodesFiltered = true;
    }
    if (!bGeometryBuilt)
    {
        if (!BuildGeometry(OutError, InCancel)) return Failure();
        VoxelGenerationAllocation::FreeRetainedArray(Nodes);
        bGeometryBuilt = true;
    }
    if (!FinalizeProfileResult(OutError, InCancel)) return Failure();
    bReady = true;
    return EVoxelHydrologyComponentStep::Ready;
}

const TArray<FVoxelHydrologyRegionKey>& FVoxelHydrologyComponentBuild::GetMissingTiles() const
{
    return MissingTiles;
}

uint64 FVoxelHydrologyComponentBuild::GetAllocatedBytes() const
{
    return VoxelGenerationAllocation::AllocatedBytes(Nodes) + VoxelGenerationAllocation::AllocatedBytes(MissingTiles) +
        VoxelGenerationAllocation::AllocatedBytes(PrepareChildren) + VoxelGenerationAllocation::AllocatedBytes(PrepareQueue) +
        VoxelGenerationAllocation::AllocatedBytes(PrepareRemap) + VoxelGenerationAllocation::AllocatedBytes(PrepareActive) +
        VoxelGenerationAllocation::AllocatedBytes(PrepareSelected) + VoxelGenerationAllocation::AllocatedBytes(ExteriorNodes) +
        Local.GetAllocatedBytes() + (LocalBuild.IsSet() ? LocalBuild->GetAllocatedBytes() : 0) + Result.GetAllocatedBytes();
}

const FVoxelHydrologyRiverComponent& FVoxelHydrologyComponentBuild::GetResult() const
{
    check(bReady);
    return Result;
}

FVoxelHydrologyRiverComponent FVoxelHydrologyComponentBuild::TakeResult()
{
    check(bReady);
    return MoveTemp(Result);
}

void FVoxelHydrologyComponentBuild::SetShape(FVoxelHydrologyComponentNode& Node) const
{
    const auto& Settings = Recipe->Settings;
    const uint64 Relative = FMath::Max<uint64>(1, Node.RoutePoint.Accumulation / FMath::Max<uint64>(1, Settings.RiverSourceAccumulation));
    const int32 Growth = FMath::Clamp(int32(FMath::FloorLog2(uint32(FMath::Min<uint64>(Relative, MAX_uint32)))), 0, Settings.RiverMaxGrowthLevels);
    Node.RoutePoint.WaterHalfWidth = Settings.RiverBaseHalfWidth + Growth * Settings.RiverWidthGrowthPerLevel;
    Node.RoutePoint.BedHalfWidth = FMath::Max(1, Node.RoutePoint.WaterHalfWidth - 1);
    Node.RoutePoint.Depth = Settings.RiverBaseDepth + Growth * Settings.RiverDepthGrowthPerLevel;
    Node.RoutePoint.BankWidth = Settings.RiverBankWidth;
    Node.RoutePoint.ShoreWidth = FMath::Max(Settings.RiverBankWidth + 1, Settings.RiverShoreWidth);
}

bool FVoxelHydrologyComponentBuild::ReadExteriorNode(const FIntPoint& InCell, FTileLookup InLookup,
    FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
    FVoxelHydrologyComponentNode& OutNode, FString& OutError, const TAtomic<bool>* InCancel)
{
    FVoxelHydrologyRegionSample Sample;
    FIntPoint World;
    if (FVoxelHydrologyMacroInput::Read(Recipe->Settings, InCell, InLookup, InRequestTile, Sample, World, OutError) != EVoxelHydrologyClosureRead::Ready) return false;
    const int32 Side = Recipe->Settings.HydrologyRegionSide;
    const FVoxelHydrologyRegionKey Key{FIntPoint(VoxelGeneration::FloorDivide(InCell.X, Side), VoxelGeneration::FloorDivide(InCell.Y, Side))};
    if (ExteriorRunoff.IsSet() && ExteriorRunoff->Cell != InCell)
    {
        OutError = TEXT("HydrologyComponentConflict: exterior runoff cursor changed before commit");
        return false;
    }
    TSharedPtr<const FVoxelHydrologyNetworkRunoff, ESPMode::ThreadSafe> Outside;
    if (!ExteriorRunoff.IsSet())
    {
        if (!Cache.AdvanceHydrologyRunoff(Key, Config, InLookup, InRequestTile, Outside, OutError, InCancel)) return false;
        const auto* Control = Outside->Find(InCell);
        if (!Control || Outside->Coordinate != Key.Coordinate || Outside->Generation != Generation || !Outside->Fingerprint)
        {
            OutError = TEXT("HydrologyComponentConflict: regional exit has no matching neighboring control");
            return false;
        }
        ExteriorRunoff.Emplace(FExteriorRunoffSnapshot{InCell, Outside->Coordinate, Outside->Generation, Outside->Fingerprint, *Control});
    }
    const auto& Control = ExteriorRunoff->Control;
#if WITH_DEV_AUTOMATION_TESTS
    if (bTestPauseAfterExteriorSnapshot)
    {
        bTestPauseAfterExteriorSnapshot = false;
        OutError = TEXT("Yielded: test exterior runoff snapshot");
        return false;
    }
#endif
    FVoxelHydrologyComponentNode Node;
    Node.Cell = World;
    Node.RoutePoint.Position = World;
    Node.GroundZ = Sample.GroundZ;
    Node.bReceiver = Sample.IsReceiver();
    Node.ReceiverWaterZ = Sample.WaterZ;
    Node.ReceiverId = Sample.WaterId;
    TSharedPtr<const FVoxelHydrologyChannelPlan, ESPMode::ThreadSafe> OutsideChannels;
    if (!Cache.AdvanceHydrologyChannels(Key, Config, InLookup, InRequestTile, OutsideChannels, OutError, InCancel)) return false;
    if (OutsideChannels->Coordinate != ExteriorRunoff->Coordinate || OutsideChannels->Generation != ExteriorRunoff->Generation ||
        OutsideChannels->RunoffFingerprint != ExteriorRunoff->Fingerprint)
    {
        OutError = TEXT("HydrologyComponentConflict: exterior channel and runoff identities differ");
        return false;
    }
    const auto* Channel = OutsideChannels->Find(InCell);
    if (!Channel || !Channel->bActive || Channel->WaterZ == MAX_int32)
    {
        OutError = TEXT("HydrologyComponentConflict: exterior channel has no canonical water profile");
        return false;
    }
    Node.RoutePoint.WaterZ = Channel->WaterZ;
    Node.RoutePoint.Accumulation = Control.Accumulation;
    Node.bHasParent = Control.Control.bHasParent;
    if (Node.bHasParent)
    {
        const int64 CellSize = Recipe->Settings.HydrologyCellSize;
        Node.ParentCell = FIntPoint(
            int32(FMath::Max(VoxelGeneration::WorldCellMin, int64(Control.Control.ParentCell.X) * CellSize)),
            int32(FMath::Max(VoxelGeneration::WorldCellMin, int64(Control.Control.ParentCell.Y) * CellSize)));
    }
    Node.bGhost = true;
    SetShape(Node);
    OutNode = Node;
    return true;
}

bool FVoxelHydrologyComponentBuild::PrepareRiverNodes(FTileLookup InLookup, FVoxelHydrologyMacroInput::FRequestTile InRequestTile,
    FString& OutError, const TAtomic<bool>* InCancel)
{
    constexpr uint8 ActivePath = 1;
    constexpr uint8 SelectedSource = 2;
    constexpr uint8 IncomingChannel = 4;
    int32 Work = 256;
    const int32 Count = Local.Cells.Num();
    const auto CellAt = [&](int32 InIndex) { return Local.Minimum + FIntPoint(InIndex % Local.Width, InIndex / Local.Width); };
    const auto IndexOf = [&](const FIntPoint& InCell)
    {
        const int64 X = int64(InCell.X) - Local.Minimum.X;
        const int64 Y = int64(InCell.Y) - Local.Minimum.Y;
        return X >= 0 && Y >= 0 && X < Local.Width && Y < Local.Height ? int32(Y * Local.Width + X) : INDEX_NONE;
    };
    const auto WorldAt = [&](const FIntPoint& InCell)
    {
        return FIntPoint(int32(FMath::Max(VoxelGeneration::WorldCellMin, int64(InCell.X) * Local.CellSize)),
            int32(FMath::Max(VoxelGeneration::WorldCellMin, int64(InCell.Y) * Local.CellSize)));
    };
    while (Work-- > 0)
    {
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
        if (PrepareStage == 0)
        {
            if (!VoxelGenerationAllocation::RetainArray(PrepareChildren, Count, OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareQueue, Count, OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareRemap, Count, OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareActive, Count, OutError) ||
                !VoxelGenerationAllocation::RetainArray(Result.DrainageCells, Count, OutError)) return false;
            PrepareStage = 1;
        }
        else if (PrepareStage == 1)
        {
            if (PrepareCursor == Count) { PrepareCursor = 0; PrepareStage = 2; continue; }
            const int32 Index = PrepareCursor;
            const auto& Cell = Local.Cells[Index];
            PrepareChildren.Add(0);
            Local.Cells[Index].ChannelWaterZ = Cell.Sample.IsReceiver() ? Cell.Sample.WaterZ : Cell.Sample.GroundZ;
            Local.Cells[Index].ReceiverFloorZ = Cell.Sample.IsReceiver() ? Cell.Sample.WaterZ : MIN_int32;
            PrepareActive.Add(0);
            PrepareRemap.Add(Cell.bHasParent ? IndexOf(Cell.ParentCell) : INDEX_NONE);
            // 所有权按X/Y排序生成，不把区外端点列为已生成格。
            Result.DrainageCells.Add(Local.Minimum + FIntPoint(Index / Local.Height, Index % Local.Height));
            ++PrepareCursor;
        }
        else if (PrepareStage == 2)
        {
            if (PrepareSourceCursor == Sources->Sources.Num()) { Sources.Reset(); PrepareStage = 3; continue; }
            const int32 Index = IndexOf(Sources->Sources[PrepareSourceCursor++].Cell);
            if (Index == INDEX_NONE) { OutError = TEXT("HydrologyComponentConflict: selected source is outside its region"); return false; }
            PrepareActive[Index] |= ActivePath | SelectedSource;
        }
        else if (PrepareStage == 3)
        {
            if (PrepareCursor == Count) { PrepareCursor = 0; PrepareStage = 4; continue; }
            const int32 Index = PrepareCursor++;
            if (PrepareRemap[Index] != INDEX_NONE) ++PrepareChildren[PrepareRemap[Index]];
            const auto* Boundary = Channels->Find(CellAt(Index));
            if (Boundary) Local.Cells[Index].ReceiverFloorZ = Boundary->ReceiverFloorZ;
            if (Boundary && Boundary->bIncomingActive) PrepareActive[Index] |= ActivePath | IncomingChannel;
            if (Boundary && Boundary->bActive) Local.Cells[Index].ChannelWaterZ = Boundary->WaterZ;
        }
        else if (PrepareStage == 4)
        {
            if (PrepareCursor == Count) { PrepareCursor = 0; PrepareStage = 5; continue; }
            if (PrepareChildren[PrepareCursor] == 0) PrepareQueue.Add(PrepareCursor);
            ++PrepareCursor;
        }
        else if (PrepareStage == 5)
        {
            if (PrepareCursor == PrepareQueue.Num())
            {
                if (PrepareQueue.Num() != Count) { OutError = TEXT("HydrologyComponentConflict: local channel parent cycle"); return false; }
                VoxelGenerationAllocation::FreeRetainedArray(PrepareChildren);
                PrepareCursor = PrepareQueue.Num();
                PrepareStage = 10;
                continue;
            }
            const int32 Index = PrepareQueue[PrepareCursor++];
            if (PrepareActive[Index] & ActivePath)
            {
                ++PrepareActiveCount;
            }
            const int32 Parent = PrepareRemap[Index];
            if (Parent != INDEX_NONE)
            {
                if (PrepareActive[Index] & ActivePath)
                {
                    Local.Cells[Parent].ChannelWaterZ = FMath::Min(Local.Cells[Parent].ChannelWaterZ, Local.Cells[Index].ChannelWaterZ);
                    PrepareActive[Parent] |= IncomingChannel;
                }
                PrepareActive[Parent] |= PrepareActive[Index] & ActivePath;
                if (--PrepareChildren[Parent] == 0) PrepareQueue.Add(Parent);
            }
        }
        else if (PrepareStage == 10)
        {
            if (PrepareCursor == 0)
            {
                VoxelGenerationAllocation::FreeRetainedArray(PrepareQueue);
                PrepareStage = 11;
                continue;
            }
            const int32 Index = PrepareQueue[--PrepareCursor];
            auto& Cell = Local.Cells[Index];
            const int32 Parent = PrepareRemap[Index];
            if (Parent != INDEX_NONE)
            {
                const int32 Floor = Local.Cells[Parent].ReceiverFloorZ;
                if (Floor == MIN_int32 || (Cell.ReceiverFloorZ != MIN_int32 && Cell.ReceiverFloorZ != Floor))
                {
                    OutError = TEXT("HydrologyComponentConflict: local receiver levels disagree");
                    return false;
                }
                Cell.ReceiverFloorZ = Floor;
            }
            if (Cell.ReceiverFloorZ == MIN_int32)
            {
                OutError = TEXT("HydrologyComponentConflict: local drainage has no receiver level");
                return false;
            }
            Cell.ChannelWaterZ = FMath::Max(Cell.ChannelWaterZ, Cell.ReceiverFloorZ);
        }
        else if (PrepareStage == 11)
        {
            if (PrepareCursor == Count)
            {
                Channels.Reset();
                PrepareCursor = 0;
                PrepareStage = 6;
                continue;
            }
            const int32 Index = PrepareCursor++;
            if (!(PrepareActive[Index] & ActivePath)) continue;
            const auto& Cell = Local.Cells[Index];
            const auto* Boundary = Channels->Find(CellAt(Index));
            if ((Boundary && (!Boundary->bActive || Cell.ChannelWaterZ != Boundary->WaterZ)) ||
                (Cell.Sample.IsReceiver() && Cell.ChannelWaterZ != Cell.Sample.WaterZ))
            {
                OutError = TEXT("HydrologyComponentConflict: local river profile disagrees with canonical control");
                return false;
            }
        }
        else if (PrepareStage == 6)
        {
            if (PrepareCursor == 0 && !VoxelGenerationAllocation::RetainArray(PrepareSelected,
                PrepareActiveCount + 2 * (Local.Width + Local.Height), OutError)) return false;
            if (PrepareCursor == Count) { PrepareCursor = 0; PrepareStage = 7; continue; }
            const int32 Index = PrepareCursor;
            if (!(PrepareActive[Index] & ActivePath)) { PrepareRemap[Index] = INDEX_NONE; ++PrepareCursor; continue; }
            const auto& Cell = Local.Cells[Index];
            const FIntPoint Position = CellAt(Index);
            FVoxelHydrologyComponentNode Node;
            Node.Cell = WorldAt(Position);
            Node.RoutePoint.Position = Node.Cell;
            Node.RoutePoint.WaterZ = Cell.ChannelWaterZ;
            Node.RoutePoint.Accumulation = Cell.Accumulation;
            Node.ParentCell = Cell.bHasParent ? WorldAt(Cell.ParentCell) : FIntPoint::ZeroValue;
            Node.Parent = Cell.bHasParent ? IndexOf(Cell.ParentCell) : INDEX_NONE;
            Node.bHasParent = Cell.bHasParent;
            Node.bReceiver = Cell.Sample.IsReceiver();
            Node.GroundZ = Cell.Sample.GroundZ;
            Node.ReceiverWaterZ = Cell.Sample.WaterZ;
            Node.ReceiverId = Cell.Sample.WaterId;
            Node.bChannelEligible = true;
            Node.bSelected = (PrepareActive[Index] & SelectedSource) != 0;
            Node.bHeadwater = Node.bSelected && !(PrepareActive[Index] & IncomingChannel);
            SetShape(Node);
            if (Node.bHasParent && Node.Parent == INDEX_NONE)
            {
                const FIntPoint ParentCell = Local.Cells[Index].ParentCell;
                const int32* Existing = ExteriorNodes.Find(ParentCell);
                if (!Existing)
                {
                    FVoxelHydrologyComponentNode Exterior;
                    if (!ReadExteriorNode(ParentCell, InLookup, InRequestTile, Exterior, OutError, InCancel)) return false;
                    if (!VoxelGenerationAllocation::RetainMap(ExteriorNodes, ExteriorNodes.Num() + 1, OutError)) return false;
                    const int32 ExteriorIndex = PrepareSelected.Add(Exterior);
                    Existing = &ExteriorNodes.Add(ParentCell, ExteriorIndex);
                    ExteriorRunoff.Reset();
                }
                Node.Parent = *Existing;
            }
            PrepareRemap[Index] = PrepareSelected.Add(Node);
            ++PrepareCursor;
        }
        else if (PrepareStage == 7)
        {
            if (PrepareCursor == Count) { PrepareStage = 8; continue; }
            const int32 Index = PrepareCursor++;
            const auto& Cell = Local.Cells[Index];
            const int32 OriginalParent = Cell.bHasParent ? IndexOf(Cell.ParentCell) : INDEX_NONE;
            if (PrepareRemap[Index] != INDEX_NONE && OriginalParent != INDEX_NONE)
            {
                const int32 Parent = PrepareRemap[OriginalParent];
                if (Parent == INDEX_NONE) { OutError = TEXT("HydrologyComponentConflict: active channel lost its real downstream"); return false; }
                PrepareSelected[PrepareRemap[Index]].Parent = Parent;
            }
        }
        else if (PrepareStage == 8)
        {
            VoxelGenerationAllocation::FreeRetainedArray(Nodes);
            Nodes = MoveTemp(PrepareSelected);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareChildren);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareQueue);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareRemap);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareActive);
            VoxelGenerationAllocation::FreeRetainedMap(ExteriorNodes);
            VoxelGenerationAllocation::FreeRetainedArray(Local.Cells);
            Graph.Reset();
            Runoff.Reset();
            Sources.Reset();
            Channels.Reset();
            PrepareStage = 9;
        }
        else { OutError.Reset(); return true; }
    }
    OutError = TEXT("Yielded: regional source and channel geometry preparation");
    return false;
}

uint64 FVoxelHydrologyRiverComponent::GetAllocatedBytes() const
{
    return Plan.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(DrainageCells);
}

bool FVoxelHydrologyRiverComponent::HasEquivalentOutput(const FVoxelHydrologyRiverComponent& InOther) const
{
    if (Key != InOther.Key || Fingerprint != InOther.Fingerprint ||
        DrainageCells != InOther.DrainageCells || Plan.Grid.CellSize != InOther.Plan.Grid.CellSize ||
        Plan.bCompleteRiverIndex != InOther.Plan.bCompleteRiverIndex ||
        Plan.Rivers.Num() != InOther.Plan.Rivers.Num() ||
        Plan.Waterfalls.Num() != InOther.Plan.Waterfalls.Num() ||
        TotalRainWeight != InOther.TotalRainWeight || IncomingRainWeight != InOther.IncomingRainWeight ||
        ReceivedRainWeight != InOther.ReceivedRainWeight || ExportedRainWeight != InOther.ExportedRainWeight) return false;
    auto SamePoint = [](const auto& A, const auto& B)
    {
        return A.Position == B.Position && A.WaterZ == B.WaterZ &&
            A.BedHalfWidth == B.BedHalfWidth && A.WaterHalfWidth == B.WaterHalfWidth &&
            A.BankWidth == B.BankWidth && A.ShoreWidth == B.ShoreWidth &&
            A.Depth == B.Depth && A.Accumulation == B.Accumulation;
    };
    for (int32 Index = 0; Index < Plan.Rivers.Num(); ++Index)
    {
        const auto& A = Plan.Rivers[Index];
        const auto& B = InOther.Plan.Rivers[Index];
        if (!(A.Id == B.Id) || !(A.OwnerRegion == B.OwnerRegion) ||
            A.bContinuation != B.bContinuation || A.Min != B.Min || A.Max != B.Max ||
            A.Points.Num() != B.Points.Num() || A.ShapePoints.Num() != B.ShapePoints.Num() ||
            A.MeanderPoints.Num() != B.MeanderPoints.Num()) return false;
        for (int32 Point = 0; Point < A.Points.Num(); ++Point)
            if (!SamePoint(A.Points[Point], B.Points[Point])) return false;
        for (int32 Point = 0; Point < A.ShapePoints.Num(); ++Point)
            if (!SamePoint(A.ShapePoints[Point], B.ShapePoints[Point]) ||
                A.ShapePoints[Point].bAnchor != B.ShapePoints[Point].bAnchor) return false;
        for (int32 Point = 0; Point < A.MeanderPoints.Num(); ++Point)
        {
            const auto& X = A.MeanderPoints[Point];
            const auto& Y = B.MeanderPoints[Point];
            if (!SamePoint(X, Y) || X.bAnchor != Y.bAnchor ||
                X.MeanderOffset != Y.MeanderOffset || X.Curvature != Y.Curvature || X.bWalkableMargins != Y.bWalkableMargins) return false;
        }
    }
    for (int32 Index = 0; Index < Plan.Waterfalls.Num(); ++Index)
    {
        const auto& A = Plan.Waterfalls[Index];
        const auto& B = InOther.Plan.Waterfalls[Index];
        if (!(A.Id == B.Id) || A.Upstream != B.Upstream || A.Downstream != B.Downstream ||
            A.HalfWidthCells != B.HalfWidthCells) return false;
    }
    return true;
}

bool FVoxelHydrologyRiverComponent::Sample(int32 InWorldX, int32 InWorldY, int32 InOriginalGround,
    FVoxelHydrologyInfluence& OutInfluence) const
{
    return Plan.SampleRiverInfluence(InWorldX, InWorldY, InOriginalGround, OutInfluence);
}

bool FVoxelHydrologyRiverComponent::ContainsCell(const FIntPoint& InCell) const
{
    int32 First = 0;
    int32 Last = DrainageCells.Num();
    while (First < Last)
    {
        const int32 Middle = First + (Last - First) / 2;
        if (HydrologyComponentCellLess(DrainageCells[Middle], InCell))
        {
            First = Middle + 1;
        }
        else
        {
            Last = Middle;
        }
    }
    return First < DrainageCells.Num() && DrainageCells[First] == InCell;
}

bool FVoxelHydrologyComponentBuild::CollectCoverageSeeds(const FVoxelHydrologyRegionKey& InCoverage,
    const FVoxelGenerationSettings& InSettings, FTileLookup InLookup,
    TArray<FIntPoint>& OutSeeds, TArray<FVoxelHydrologyRegionKey>& OutMissingTiles, FString& OutError)
{

    const int64 Size = InSettings.HydrologyCellSize;
    if (Size <= 0) { OutError = TEXT("Invalid hydrology coverage cell size"); return false; }
    const int64 Margin = int64(InSettings.RiverBaseHalfWidth) +
        int64(InSettings.RiverMaxGrowthLevels) * InSettings.RiverWidthGrowthPerLevel +
        InSettings.RiverBankWidth + FMath::Max(InSettings.RiverBankWidth + 1, InSettings.RiverShoreWidth) +
        InSettings.RiverMeanderStrength + Size;
    const int64 CoreX = int64(InCoverage.Coordinate.X) * VoxelHydrology::CoverageSide;
    const int64 CoreY = int64(InCoverage.Coordinate.Y) * VoxelHydrology::CoverageSide;
    const int64 MinX = FMath::Max(VoxelGeneration::WorldCellMin, CoreX - Margin);
    const int64 MinY = FMath::Max(VoxelGeneration::WorldCellMin, CoreY - Margin);
    const int64 MaxX = FMath::Min(VoxelGeneration::WorldCellMaxExclusive, CoreX + VoxelHydrology::CoverageSide + Margin);
    const int64 MaxY = FMath::Min(VoxelGeneration::WorldCellMaxExclusive, CoreY + VoxelHydrology::CoverageSide + Margin);
    if (MinX >= MaxX || MinY >= MaxY) { OutError = TEXT("Invalid hydrology coverage outside world domain"); return false; }
    const int64 BeginX = VoxelGeneration::FloorDivideWide(MinX, Size);
    const int64 BeginY = VoxelGeneration::FloorDivideWide(MinY, Size);
    const int64 EndX = VoxelGeneration::FloorDivideWide(MaxX - 1, Size) + 1;
    const int64 EndY = VoxelGeneration::FloorDivideWide(MaxY - 1, Size) + 1;
    const int64 Count = (EndX - BeginX) * (EndY - BeginY);
    if (Count <= 0 || Count > MAX_int32) { OutError = TEXT("Oversized: hydrology coverage seeds"); return false; }
    if (!VoxelGenerationAllocation::RetainArray(OutSeeds, int32(Count), OutError)) return false;
    OutSeeds.Reset();
    OutMissingTiles.Reset();
    for (int64 X = BeginX; X < EndX; ++X)
        for (int64 Y = BeginY; Y < EndY; ++Y) OutSeeds.Add(FIntPoint(int32(X), int32(Y)));
    OutError.Reset();
    return true;
}
