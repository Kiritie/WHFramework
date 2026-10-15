#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelGenerationPlanCache.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"

namespace
{
    int32 HydrologyComponentCapacity(const int32 InRequired, const int32 InCapacity)
    {
        return int32(FMath::Min<int64>(MAX_int32,
            FMath::Max<int64>(InRequired, FMath::Max<int64>(4, int64(InCapacity) * 2))));
    }

    bool IsHydrologyComponentCell(const FIntPoint& InCell, const int32 InCellSize)
    {
        const int64 Minimum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InCellSize);
        const int64 Maximum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InCellSize);
        return InCell.X >= Minimum && InCell.Y >= Minimum && InCell.X <= Maximum && InCell.Y <= Maximum;
    }

    bool IsHydrologyComponentRegion(const FVoxelHydrologyRegionKey& InKey,
        const FVoxelGenerationSettings& InSettings)
    {
        const int64 Minimum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin, InSettings.HydrologyCellSize);
        const int64 Maximum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1, InSettings.HydrologyCellSize) + 1;
        const int64 StartX = int64(InKey.Coordinate.X) * InSettings.HydrologyRegionSide;
        const int64 StartY = int64(InKey.Coordinate.Y) * InSettings.HydrologyRegionSide;
        return StartX < Maximum && StartY < Maximum &&
            StartX + InSettings.HydrologyRegionSide > Minimum &&
            StartY + InSettings.HydrologyRegionSide > Minimum;
    }

    bool HydrologyComponentCellLess(const FIntPoint& InFirst, const FIntPoint& InSecond)
    {
        return InFirst.X < InSecond.X || (InFirst.X == InSecond.X && InFirst.Y < InSecond.Y);
    }
}

FVoxelHydrologyComponentBuild::FVoxelHydrologyComponentBuild(
    TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
    TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
    const FIntPoint& InSeedCell)
    : Recipe(MoveTemp(InRecipe))
    , Terrain(MoveTemp(InTerrain))
    , SeedCell(InSeedCell)
{
}

FVoxelHydrologyComponentBuild::~FVoxelHydrologyComponentBuild()
{
}

void FVoxelHydrologyComponentBuild::VisitFineSources(FVoxelGenerationRequiredSourceScope::FVisit InVisit) const
{
    if (PinnedFinePlan)
    {
        FVoxelGenerationRequiredSourceScope::VisitLease(PinnedFinePlan->ResourceLease.Get(), InVisit);
    }
}

bool FVoxelHydrologyComponentBuild::AddMissingTile(const FVoxelHydrologyRegionKey& InKey, FString& OutError)
{
    if (MissingTiles.Contains(InKey))
    {
        return true;
    }
    if (MissingTiles.Num() == MissingTiles.Max() &&
        !VoxelGenerationAllocation::RetainArray(MissingTiles,
            HydrologyComponentCapacity(MissingTiles.Num() + 1, MissingTiles.Max()), OutError))
    {
        return false;
    }
    MissingTiles.Add(InKey);
    return true;
}

bool FVoxelHydrologyComponentBuild::InitializeRouting(FString& OutError)
{
    if (bRoutingInitialized) return true;
    const auto& S = Recipe->Settings;
    RoutingShape.WaterHalfWidth = S.RiverBaseHalfWidth;
    RoutingShape.BedHalfWidth = FMath::Max(1,S.RiverBaseHalfWidth-1);
    RoutingShape.Depth = S.RiverBaseDepth;
    RoutingShape.BankWidth = S.RiverBankWidth;
    RoutingShape.ShoreWidth = FMath::Max(S.RiverBankWidth+1,S.RiverShoreWidth);
    RoutingShape.MaxCutFill = FMath::Max(64,RoutingShape.Depth*4);
    const int64 JunctionK = int64(RoutingShape.WaterHalfWidth)+RoutingShape.BankWidth+RoutingShape.ShoreWidth+1;
    const int64 MaximumRadius = FMath::Max<int64>(JunctionK*2,
        int64(S.RiverBaseHalfWidth)+int64(S.RiverMaxGrowthLevels)*S.RiverWidthGrowthPerLevel+
        RoutingShape.BankWidth+RoutingShape.ShoreWidth+S.RiverMeanderStrength);
    if (MaximumRadius > MAX_int32)
    { OutError = TEXT("Oversized: routing footprint radius"); return false; }
    if (!RoutingGrid.Initialize(S.HydrologyCellSize,int32(MaximumRadius),
        FIntPoint(int32(VoxelGeneration::WorldCellMin),int32(VoxelGeneration::WorldCellMin)),
        FIntPoint(int32(VoxelGeneration::WorldCellMaxExclusive-1),int32(VoxelGeneration::WorldCellMaxExclusive-1)),
        FIntPoint::ZeroValue,OutError)) return false;
    if (!WidthRouting.Initialize(RoutingGrid,RoutingShape,FMath::Max(-32768,S.MinZ),
        FIntPoint(int32(VoxelGeneration::WorldCellMin),int32(VoxelGeneration::WorldCellMin)),
        FIntPoint(int32(VoxelGeneration::WorldCellMaxExclusive-1),int32(VoxelGeneration::WorldCellMaxExclusive-1)),
        Recipe->RecipeHash,OutError)) return false;
    bRoutingInitialized = true; return true;
}

bool FVoxelHydrologyComponentBuild::ReadRoutingFacts(const FIntPoint& InWorld, FTileLookup InLookup,
    FVoxelHydrologyComponentNode& OutNode, FString& OutError)
{
    if (!InitializeRouting(OutError)) return false;
    FVoxelHydrologyReceivingVoxel Fine;
    if (ReadFineWater(InWorld,InLookup,Fine,OutError) != EVoxelHydrologyClosureRead::Ready) return false;
    uint64 Rain = 0;
    if (RoutingGrid.IsGridNode(InWorld))
    {
        const uint64* Existing = RoutingRain.Find(InWorld);
        if (!Existing)
        {
            if (!bRoutingRainActive)
            {
                if (!RoutingGrid.TryGetCoarseBounds(InWorld,RoutingRainMin,RoutingRainMax))
                { OutError = TEXT("Invalid routing rainfall ownership"); return false; }
                RoutingRainSeed = InWorld; RoutingRainNext = RoutingRainMin;
                RoutingRainTotal = 0; bRoutingRainActive = true;
            }
            int32 Work = 256;
            while (RoutingRainNext.Y <= RoutingRainMax.Y && Work > 0)
            {
                --Work;
                if (ActiveCancel && ActiveCancel->Load()) { OutError = TEXT("Canceled"); return false; }
                const auto& S = Recipe->Settings;
                const FVoxelHydrologyRegionKey Owner{FIntPoint(
                    VoxelGeneration::FloorDivide(RoutingRainNext.X,S.HydrologyRegionSide),
                    VoxelGeneration::FloorDivide(RoutingRainNext.Y,S.HydrologyRegionSide))};
                const FVoxelHydrologyPlan* Tile = InLookup(Owner);
                if (!Tile) { AddMissingTile(Owner,OutError); return false; }
                const FIntPoint Local = RoutingRainNext-Tile->Grid.WorldMinCell;
                if (Local.X<0 || Local.Y<0 || Local.X>=Tile->Grid.Width || Local.Y>=Tile->Grid.Height)
                { OutError = TEXT("Canonical rainfall tile misses its owned cell"); return false; }
                const uint32 Index = Tile->Grid.ToIndex(Local.X,Local.Y);
                if (!Tile->Grid.RainWeight.IsValidIndex(int32(Index)))
                { OutError = TEXT("Incomplete canonical rainfall facts"); return false; }
                const uint64 Value = Tile->Grid.RainWeight[Index];
                if (MAX_uint64-RoutingRainTotal < Value)
                { OutError = TEXT("Oversized: routing rainfall sum"); return false; }
                RoutingRainTotal += Value;
                if (RoutingRainNext.X == RoutingRainMax.X)
                { RoutingRainNext.X = RoutingRainMin.X; ++RoutingRainNext.Y; }
                else ++RoutingRainNext.X;
            }
            if (RoutingRainNext.Y <= RoutingRainMax.Y)
            { OutError = TEXT("Yielded: routing rainfall aggregation"); return false; }
            if (RoutingRain.Num() == RoutingRainCapacity)
            {
                if (RoutingRainCapacity == MAX_int32) { OutError = TEXT("Oversized: routing rainfall cache"); return false; }
                const int32 Capacity = HydrologyComponentCapacity(RoutingRainCapacity+1,RoutingRainCapacity);
                if (!VoxelGenerationAllocation::RetainMap(RoutingRain,Capacity,OutError)) return false;
                RoutingRainCapacity = Capacity;
            }
            RoutingRain.Add(RoutingRainSeed,RoutingRainTotal); bRoutingRainActive = false;
            Existing = RoutingRain.Find(InWorld);
            if (!Existing) { OutError = TEXT("Yielded: another routing rainfall bucket"); return false; }
        }
        Rain = *Existing;
    }
    FVoxelHydrologyComponentNode Node;
    Node.Cell = InWorld; Node.RoutePoint.Position = InWorld;
    Node.GroundZ = Fine.OriginalGroundZ; Node.RainWeight = Rain;
    Node.bReceiver = Fine.WaterZ != MIN_int32; Node.ReceiverWaterZ = Fine.WaterZ; Node.ReceiverId = Fine.BodyId;
    Node.RoutePoint.WaterHalfWidth = RoutingShape.WaterHalfWidth;
    Node.RoutePoint.BedHalfWidth = RoutingShape.BedHalfWidth;
    Node.RoutePoint.Depth = RoutingShape.Depth;
    Node.RoutePoint.BankWidth = RoutingShape.BankWidth;
    Node.RoutePoint.ShoreWidth = RoutingShape.ShoreWidth;
    OutNode = Node; OutError.Reset(); return true;
}

bool FVoxelHydrologyComponentBuild::ReadRawNode(const FIntPoint& InCell, FTileLookup InLookup,
    FVoxelHydrologyComponentNode& OutNode, FString& OutError)
{
    const FVoxelGenerationSettings& Settings = Recipe->Settings;
    if (!IsHydrologyComponentCell(InCell, Settings.HydrologyCellSize))
    {
        OutError = TEXT("Invalid hydrology component cell outside world domain");
        return false;
    }
    const FVoxelHydrologyRegionKey Owner{FIntPoint(
        VoxelGeneration::FloorDivide(InCell.X, Settings.HydrologyRegionSide),
        VoxelGeneration::FloorDivide(InCell.Y, Settings.HydrologyRegionSide))};
    const FVoxelHydrologyPlan* Tile = InLookup(Owner);
    if (!Tile)
    {
        AddMissingTile(Owner, OutError);
        return false;
    }
    const FIntPoint Local = InCell - Tile->Grid.WorldMinCell;
    if (Local.X < 0 || Local.Y < 0 || Local.X >= Tile->Grid.Width || Local.Y >= Tile->Grid.Height)
    {
        OutError = TEXT("Canonical hydrology tile does not contain its owned cell");
        return false;
    }
    const uint32 Index = Tile->Grid.ToIndex(Local.X, Local.Y);
    if (!Tile->Drainage.Parent.IsValidIndex(int32(Index)) ||
        !Tile->Drainage.SpillPlane.IsValidIndex(int32(Index)) ||
        !Tile->Drainage.Accumulation.IsValidIndex(int32(Index)) ||
        !Tile->Grid.GroundPlane.IsValidIndex(int32(Index)) ||
        !Tile->Grid.RainWeight.IsValidIndex(int32(Index)))
    {
        OutError = TEXT("Canonical hydrology tile has incomplete drainage");
        return false;
    }
    FVoxelHydrologyComponentNode Node;
    Node.Cell = InCell;
    if (!Tile->Grid.TryToWorldVoxelXY(Index, Node.RoutePoint.Position))
    {
        OutError = TEXT("Canonical hydrology node has no world position");
        return false;
    }
    Node.GroundZ = Tile->Grid.GroundPlane[Index];
    Node.RainWeight = Tile->Grid.RainWeight[Index];
    Node.RoutePoint.WaterZ = Tile->Drainage.SpillPlane[Index];
    Node.RoutePoint.Accumulation = Tile->Drainage.Accumulation[Index];
    const uint64 Threshold = FMath::Max<uint64>(1, Settings.RiverSourceAccumulation);
    const uint64 Relative = FMath::Max<uint64>(1, Node.RoutePoint.Accumulation / Threshold);
    const int32 Growth = FMath::Clamp(int32(FMath::FloorLog2(
        uint32(FMath::Min<uint64>(Relative, MAX_uint32)))), 0, Settings.RiverMaxGrowthLevels);
    Node.RoutePoint.WaterHalfWidth = Settings.RiverBaseHalfWidth + Growth * Settings.RiverWidthGrowthPerLevel;
    Node.RoutePoint.BedHalfWidth = FMath::Max(1, Node.RoutePoint.WaterHalfWidth - 1);
    Node.RoutePoint.BankWidth = Settings.RiverBankWidth;
    Node.RoutePoint.ShoreWidth = FMath::Max(Settings.RiverBankWidth + 1, Settings.RiverShoreWidth);
    Node.RoutePoint.Depth = Settings.RiverBaseDepth + Growth * Settings.RiverDepthGrowthPerLevel;
    const int32 Parent = Tile->Drainage.Parent[Index];
    Node.bHasParent = Parent >= 0;
    if (Node.bHasParent)
    {
        if (!Tile->Grid.IsValidIndex(uint32(Parent)))
        {
            OutError = TEXT("Canonical hydrology node has an invalid parent");
            return false;
        }
        Node.ParentCell = Tile->Grid.ToWorldHydrologyCell(uint32(Parent));
    }
    Node.bSelected = Tile->HasSelectedRiverCell(InCell);
    if (const int32* Ocean = Tile->Grid.OceanOutlets.Find(Index))
    {
        Node.bReceiver = true;
        Node.ReceiverWaterZ = *Ocean;
        Node.ReceiverId = VoxelGeneration::MakeStableId(Settings.Seed,
            FIntVector(0, 0, Settings.SeaLevel), 0x4F4345414E524543ull, 0);
    }

    const int32 OwnerRadius = int32((int64(Settings.HydrologyHaloCells) +
        Settings.HydrologyRegionSide - 1) / Settings.HydrologyRegionSide);
    bool bMissing = false;
    bool bHasLakeReceiver = false;
    for (int32 OffsetY = -OwnerRadius; OffsetY <= OwnerRadius; ++OffsetY)
    {
        for (int32 OffsetX = -OwnerRadius; OffsetX <= OwnerRadius; ++OffsetX)
        {
            const int64 LakeOwnerX = int64(Owner.Coordinate.X) + OffsetX;
            const int64 LakeOwnerY = int64(Owner.Coordinate.Y) + OffsetY;
            if (LakeOwnerX < MIN_int32 || LakeOwnerY < MIN_int32 || LakeOwnerX > MAX_int32 || LakeOwnerY > MAX_int32)
            {
                continue;
            }
            const FVoxelHydrologyRegionKey LakeOwner{FIntPoint(int32(LakeOwnerX), int32(LakeOwnerY))};
            if (!IsHydrologyComponentRegion(LakeOwner, Settings))
            {
                continue;
            }
            const FVoxelHydrologyPlan* LakeTile = InLookup(LakeOwner);
            if (!LakeTile)
            {
                if (!AddMissingTile(LakeOwner, OutError))
                {
                    return false;
                }
                bMissing = true;
                continue;
            }
            for (const FVoxelLakePlan& Lake : LakeTile->Lakes)
            {
                if (Lake.Cells.Contains(Node.RoutePoint.Position))
                {
                    if (!Node.bReceiver || Lake.WaterZ > Node.ReceiverWaterZ ||
                        (Lake.WaterZ == Node.ReceiverWaterZ && (!bHasLakeReceiver || Lake.Id < Node.ReceiverId)))
                    {
                        Node.ReceiverWaterZ = Lake.WaterZ;
                        Node.ReceiverId = Lake.Id;
                        bHasLakeReceiver = true;
                    }
                    Node.bReceiver = true;
                }
            }
        }
    }
    if (bMissing)
    {
        return false;
    }
    if (Node.bReceiver)
    {
        Node.bHasParent = false;
    }
    OutNode = Node;
    OutError.Reset();
    return true;
}

void FVoxelHydrologyComponentBuild::BeginFineInputStep(const TAtomic<bool>* InCancel)
{
    ActiveCancel = InCancel;
    MissingTiles.Reset();
}

EVoxelHydrologyClosureRead FVoxelHydrologyComponentBuild::ReadRawFineCell(const FIntPoint& InCell,
    FTileLookup InLookup, FVoxelHydrologySharedCell& OutCell, FString& OutError)
{
    if (!VoxelGeneration::IsWorldCell(InCell.X) || !VoxelGeneration::IsWorldCell(InCell.Y))
    { OutError = TEXT("Fine receiving-water read outside physical world"); return EVoxelHydrologyClosureRead::Failed; }
    FVoxelHydrologySharedCell Value;
    Value.Cell = InCell;
    if (const auto* Fine = FineWaterCache.Find(InCell)) Value.GroundZ = Fine->OriginalGroundZ;
    else if (const int32* Ground = RawGroundCache.Find(InCell)) Value.GroundZ = *Ground;
    else
    {
        Value.GroundZ = Terrain->SampleSurfaceHeight(InCell.X, InCell.Y);


        constexpr int32 CacheLimit = 8192;
        if (!bRawGroundCacheDisabled)
        {
            if (RawGroundRing.Num()<CacheLimit && RawGroundRing.Num()==RawGroundCapacity)
            {
                const int32 Capacity=FMath::Min(CacheLimit,HydrologyComponentCapacity(RawGroundCapacity+1,RawGroundCapacity));
                FString CacheError;
                if (!VoxelGenerationAllocation::RetainMap(RawGroundCache,Capacity,CacheError) ||
                    !VoxelGenerationAllocation::RetainArray(RawGroundRing,Capacity,CacheError))
                {


                    VoxelGenerationAllocation::FreeRetainedMap(RawGroundCache);
                    VoxelGenerationAllocation::FreeRetainedArray(RawGroundRing);
                    RawGroundCapacity=RawGroundEvict=0; bRawGroundCacheDisabled=true;
                    if (!CacheError.StartsWith(TEXT("CapacityBlocked")))
                    { OutError=CacheError; return EVoxelHydrologyClosureRead::Failed; }
                }
                else RawGroundCapacity=Capacity;
            }
            if (!bRawGroundCacheDisabled)
            {
                if (RawGroundRing.Num()==CacheLimit)
                {
                    RawGroundCache.Remove(RawGroundRing[RawGroundEvict]);
                    RawGroundRing[RawGroundEvict]=InCell;
                    RawGroundEvict=(RawGroundEvict+1)%CacheLimit;
                }
                else RawGroundRing.Add(InCell);
                RawGroundCache.Add(InCell,Value.GroundZ);
            }
        }
    }
    if (Value.GroundZ < Recipe->Settings.SeaLevel)
    {
        Value.ReceiverWaterZ = Recipe->Settings.SeaLevel;
        Value.ReceiverId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
            FIntVector(0, 0, Recipe->Settings.SeaLevel), 0x4F4345414E524543ull, 0);
    }
    else
    {
        const FIntPoint Hydro(VoxelGeneration::FloorDivide(InCell.X, Recipe->Settings.HydrologyCellSize),
            VoxelGeneration::FloorDivide(InCell.Y, Recipe->Settings.HydrologyCellSize));
        FVoxelHydrologyComponentNode Raw;
        if (!ReadRawNode(Hydro, InLookup, Raw, OutError))
            return OutError.IsEmpty() && !MissingTiles.IsEmpty() ? EVoxelHydrologyClosureRead::Pending : EVoxelHydrologyClosureRead::Failed;
        if (Raw.bReceiver && Value.GroundZ < Raw.ReceiverWaterZ)
        { Value.ReceiverWaterZ = Raw.ReceiverWaterZ; Value.ReceiverId = Raw.ReceiverId; }
    }
    OutCell = Value;
    OutError.Reset();
    return EVoxelHydrologyClosureRead::Ready;
}

EVoxelHydrologyClosureRead FVoxelHydrologyComponentBuild::ReadCertifiedFineCell(const FIntPoint& InCell,
    FTileLookup InLookup, FVoxelHydrologySharedCell& OutCell, FString& OutError,
    FVoxelGenerationPlanCache* InCertificateCache)
{
    if (ActiveCancel && ActiveCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyClosureRead::Failed; }
    const auto PublishCell = [&](const FVoxelHydrologySharedCell& InValue)
    {
        OutCell = InValue;
        if (!OutCell.IsReceiver() && InCertificateCache)
        {
            if (!PinnedFinePlan || !PinnedFinePlan->Contains(InCell))
                InCertificateCache->FindFineWater(InCell, PinnedFinePlan);
            if (PinnedFinePlan && !PinnedFinePlan->SampleWater(InCell).IsWet())
                OutCell.bHasCertifiedDryOutlet = true;
        }
        OutError.Reset();
        return EVoxelHydrologyClosureRead::Ready;
    };
    if (const auto* Existing = CertifiedFineRoots.Find(InCell))
        return PublishCell(*Existing);
    FVoxelHydrologySharedCell Raw;
    const auto Read = ReadRawFineCell(InCell, InLookup, Raw, OutError);
    if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
    const auto OceanId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
        FIntVector(0, 0, Recipe->Settings.SeaLevel), 0x4F4345414E524543ull, 0);
    if (!Raw.IsReceiver() || Raw.ReceiverId == OceanId)
        return PublishCell(Raw);
    if (const auto* Existing = CertifiedFineRoots.Find(InCell))
        return PublishCell(*Existing);
    if (!bLakeCertificateActive)
    {
        const int32 Minimum = int32(VoxelGeneration::WorldCellMin);
        const int32 Maximum = int32(VoxelGeneration::WorldCellMaxExclusive - 1);
        if (!LakeCertificate.Initialize(Raw, OceanId, FIntPoint(Minimum, Minimum),
            FIntPoint(Maximum, Maximum), OutError)) return EVoxelHydrologyClosureRead::Failed;
        bLakeCertificateActive = true;
        LakeCertificateCopyIndex = 0;
    }
    auto Reader = [&](const FIntPoint& Cell, FVoxelHydrologySharedCell& Value, FString& Error)
    { return ReadRawFineCell(Cell, InLookup, Value, Error); };
    const auto Step = LakeCertificate.Advance(Reader, 256, OutError, ActiveCancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready)
    {
        if (Step == EVoxelHydrologySharedSearchStep::Yielded)
        { OutError = TEXT("Yielded: existing coarse lake requires a complete fine-shore certificate"); return EVoxelHydrologyClosureRead::Pending; }
        if (Step == EVoxelHydrologySharedSearchStep::NeedInput) return EVoxelHydrologyClosureRead::Pending;
        return EVoxelHydrologyClosureRead::Failed;
    }
    const auto& Cells = LakeCertificate.GetCells();
    int32 Work = 256;
    while (LakeCertificateCopyIndex < Cells.Num() && Work-- > 0)
    {
        const auto& Value = Cells[LakeCertificateCopyIndex];
        if (Value.IsReceiver() && Value.ReceiverWaterZ == LakeCertificate.GetWaterZ())
        {
            FVoxelHydrologySharedCell Certified = Value;
            if (!LakeCertificate.IsAccepted())
            { Certified.ReceiverWaterZ = MIN_int32; Certified.ReceiverId = FVoxelStableId(); }
            if (const auto* Existing = CertifiedFineRoots.Find(Value.Cell))
            {
                if (Existing->ReceiverWaterZ != Certified.ReceiverWaterZ || !(Existing->ReceiverId == Certified.ReceiverId))
                { OutError = TEXT("HydrologyComponentConflict: independent fine-lake root certificates disagree"); return EVoxelHydrologyClosureRead::Failed; }
            }
            else
            {
                if (CertifiedFineRoots.Num() == FineRootCapacity)
                {
                    if (FineRootCapacity == MAX_int32) { OutError = TEXT("Oversized: fine lake root certificates"); return EVoxelHydrologyClosureRead::Failed; }
                    const int32 Capacity = HydrologyComponentCapacity(FineRootCapacity + 1, FineRootCapacity);
                    if (!VoxelGenerationAllocation::RetainMap(CertifiedFineRoots, Capacity, OutError)) return EVoxelHydrologyClosureRead::Failed;
                    FineRootCapacity = Capacity;
                }
                CertifiedFineRoots.Add(Value.Cell, Certified);
            }
        }
        ++LakeCertificateCopyIndex;
    }
    if (LakeCertificateCopyIndex < Cells.Num())
    { OutError = TEXT("Yielded: fine lake root certificate transfer"); return EVoxelHydrologyClosureRead::Pending; }
    LakeCertificate.FreeBacking();
    bLakeCertificateActive = false;
    if (const auto* Existing = CertifiedFineRoots.Find(InCell))
        return PublishCell(*Existing);
    OutError = TEXT("Yielded: another existing lake needs fine-shore certification");
    return EVoxelHydrologyClosureRead::Pending;
}

EVoxelHydrologyClosureRead FVoxelHydrologyComponentBuild::ReadFineWater(const FIntPoint& InCell,
    FTileLookup InLookup, FVoxelHydrologyReceivingVoxel& OutWater, FString& OutError)
{
    if (ActiveCancel && ActiveCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyClosureRead::Failed; }
    if (ActiveFineCache)
    {
        check(ActiveConfig && ActiveConfig->IsValid());
        if (!PinnedFinePlan || !PinnedFinePlan->Contains(InCell))
        {
            PinnedFinePlan.Reset();
            if (!ActiveFineCache->AdvanceFineWater(InCell, ActiveConfig->ToSharedRef(), Terrain,
                PinnedFinePlan, OutError, ActiveCancel))
            {
                if (OutError.StartsWith(TEXT("Yielded")) && !OutError.StartsWith(TEXT("YieldedImmediate:")))
                {
                    OutError = TEXT("DependencyNotReady: shared fine water: ") + OutError;
                }
                return OutError.StartsWith(TEXT("Yielded")) || OutError.StartsWith(TEXT("DependencyNotReady"))
                    ? EVoxelHydrologyClosureRead::Pending : EVoxelHydrologyClosureRead::Failed;
            }
        }
        const auto Fine = PinnedFinePlan->SampleWater(InCell);
        OutWater = FVoxelHydrologyReceivingVoxel();
        OutWater.OriginalGroundZ = Terrain->SampleSurfaceHeight(InCell.X, InCell.Y);
        OutWater.WaterZ = Fine.WaterZ;
        if (Fine.bFixedWater) OutWater.BodyId = Fine.FixedId;
        else if (Fine.bNaturalWater)
            OutWater.BodyId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
                FIntVector(Fine.NaturalKey.MinimumMember.X, Fine.NaturalKey.MinimumMember.Y, Fine.NaturalKey.WaterZ),
                0x46494E454C414B45ull, 0);
        const auto OceanId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
            FIntVector(0, 0, Recipe->Settings.SeaLevel), 0x4F4345414E524543ull, 0);
        OutWater.bOcean = Fine.bFixedWater && Fine.FixedId == OceanId;
        OutError.Reset();
        return EVoxelHydrologyClosureRead::Ready;
    }
    if (const auto* Existing = FineWaterCache.Find(InCell))
    { OutWater = *Existing; OutError.Reset(); return EVoxelHydrologyClosureRead::Ready; }
    if (!bFineWaterActive)
    {
        const int32 Minimum = int32(VoxelGeneration::WorldCellMin);
        const int32 Maximum = int32(VoxelGeneration::WorldCellMaxExclusive - 1);
        if (!FineWaterQuery.Initialize(InCell, FIntPoint(Minimum, Minimum), FIntPoint(Maximum, Maximum), OutError))
            return EVoxelHydrologyClosureRead::Failed;
        FineWaterSeed = InCell;
        FineWaterCopyIndex = 0;
        FineDryCopyIndex = 0;
        bFineWaterActive = true;
    }
    auto Reader = [&](const FIntPoint& Cell, FVoxelHydrologySharedCell& Value, FString& Error)
    { return ReadCertifiedFineCell(Cell, InLookup, Value, Error); };
    auto IsOcean = [&](const FIntPoint& Cell)
    {
        return Terrain->SampleSurfaceHeight(Cell.X, Cell.Y) < Recipe->Settings.SeaLevel;
    };
    const FVoxelHydrologyFineWaterQuery::FWetHint WetHint(IsOcean);
    const auto Step = FineWaterQuery.Advance(Reader, 256, OutError, ActiveCancel, &WetHint);
    if (Step != EVoxelHydrologyFineWaterStep::Ready)
    {
        if (Step == EVoxelHydrologyFineWaterStep::Yielded)
        { OutError = TEXT("Yielded: static fine-water certificate"); return EVoxelHydrologyClosureRead::Pending; }
        if (Step == EVoxelHydrologyFineWaterStep::NeedTiles) return EVoxelHydrologyClosureRead::Pending;
        if (Step == EVoxelHydrologyFineWaterStep::NoReceiver)
            OutError = TEXT("HydrologyComponentConflict: fine terrain has no certified water receiver");
        return EVoxelHydrologyClosureRead::Failed;
    }
    const auto& Fine = FineWaterQuery.GetResult();
    FVoxelHydrologyReceivingVoxel Water;
    Water.WaterZ = Fine.WaterZ;
    Water.OriginalGroundZ = FineWaterQuery.GetSeedGround();
    if (Fine.bFixedWater) Water.BodyId = Fine.FixedId;
    else if (Fine.bNaturalWater)
        Water.BodyId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
            FIntVector(Fine.NaturalKey.MinimumMember.X, Fine.NaturalKey.MinimumMember.Y, Fine.NaturalKey.WaterZ),
            0x46494E454C414B45ull, 0);
    const auto OceanId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
        FIntVector(0, 0, Recipe->Settings.SeaLevel), 0x4F4345414E524543ull, 0);
    Water.bOcean = Fine.bFixedWater && Fine.FixedId == OceanId;
    const auto& Members = FineWaterQuery.GetCells();
    const int32 Count = Fine.bNaturalWater ? Members.Num() : 1;
    int32 Work = 256;
    while (FineWaterCopyIndex < Count && Work-- > 0)
    {
        if (ActiveCancel && ActiveCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyClosureRead::Failed; }
        const FIntPoint Cell = Fine.bNaturalWater ? Members[FineWaterCopyIndex] : FineWaterSeed;
        if (Fine.bNaturalWater) Water.OriginalGroundZ = FineWaterQuery.GetMemberGround(FineWaterCopyIndex);
        if (const auto* Existing = FineWaterCache.Find(Cell))
        {
            if (Existing->OriginalGroundZ != Water.OriginalGroundZ || Existing->WaterZ != Water.WaterZ || !(Existing->BodyId == Water.BodyId) || Existing->bOcean != Water.bOcean)
            { OutError = TEXT("HydrologyComponentConflict: static fine-water certificates disagree"); return EVoxelHydrologyClosureRead::Failed; }
        }
        else
        {
            if (FineWaterCache.Num() == FineWaterCapacity)
            {
                if (FineWaterCapacity == MAX_int32) { OutError = TEXT("Oversized: static fine-water cache"); return EVoxelHydrologyClosureRead::Failed; }
                const int32 Capacity = HydrologyComponentCapacity(FineWaterCapacity + 1, FineWaterCapacity);
                if (!VoxelGenerationAllocation::RetainMap(FineWaterCache, Capacity, OutError)) return EVoxelHydrologyClosureRead::Failed;
                FineWaterCapacity = Capacity;
            }
            FineWaterCache.Add(Cell, Water);
        }
        ++FineWaterCopyIndex;
    }
    if (FineWaterCopyIndex < Count)
    { OutError = TEXT("Yielded: static fine-water result transfer"); return EVoxelHydrologyClosureRead::Pending; }

    const auto& Drainage = FineWaterQuery.GetDrainageCertificate();
    while (FineDryCopyIndex < Drainage.Num() && Work > 0)
    {
        --Work;
        if (ActiveCancel && ActiveCancel->Load()) { OutError = TEXT("Canceled"); return EVoxelHydrologyClosureRead::Failed; }
        const auto& Cell = Drainage[FineDryCopyIndex];
        if (Cell.bIncluded && (Cell.IsReceiver() || Cell.GroundZ >= Cell.SpillZ))
        {
            FVoxelHydrologyReceivingVoxel Fact;
            Fact.OriginalGroundZ = Cell.GroundZ;
            if (Cell.IsReceiver())
            { Fact.WaterZ = Cell.ReceiverWaterZ; Fact.BodyId = Cell.ReceiverId; Fact.bOcean = Fact.BodyId == OceanId; }
            if (const auto* Existing = FineWaterCache.Find(Cell.Cell))
            {
                if (Existing->OriginalGroundZ != Fact.OriginalGroundZ || Existing->WaterZ != Fact.WaterZ || !(Existing->BodyId == Fact.BodyId) || Existing->bOcean != Fact.bOcean)
                { OutError = TEXT("HydrologyComponentConflict: certified fine dry/root facts disagree"); return EVoxelHydrologyClosureRead::Failed; }
            }
            else
            {
                if (FineWaterCache.Num() == FineWaterCapacity)
                {
                    if (FineWaterCapacity == MAX_int32) { OutError = TEXT("Oversized: static fine-water cache"); return EVoxelHydrologyClosureRead::Failed; }
                    const int32 Capacity = HydrologyComponentCapacity(FineWaterCapacity + 1, FineWaterCapacity);
                    if (!VoxelGenerationAllocation::RetainMap(FineWaterCache, Capacity, OutError)) return EVoxelHydrologyClosureRead::Failed;
                    FineWaterCapacity = Capacity;
                }
                FineWaterCache.Add(Cell.Cell, Fact);
            }
        }
        ++FineDryCopyIndex;
    }
    if (FineDryCopyIndex < Drainage.Num())
    { OutError = TEXT("Yielded: shared fine dry/root certificate transfer"); return EVoxelHydrologyClosureRead::Pending; }
    FineWaterQuery.FreeBacking();
    bFineWaterActive = false;
    if (const auto* Existing = FineWaterCache.Find(InCell))
    { OutWater = *Existing; OutError.Reset(); return EVoxelHydrologyClosureRead::Ready; }
    OutError = TEXT("Yielded: another fine-water point needs its immutable certificate");
    return EVoxelHydrologyClosureRead::Pending;
}

bool FVoxelHydrologyComponentBuild::ReadBucketOutlet(const FIntPoint& InAnchor, FTileLookup InLookup,
    FVoxelHydrologyBucketOutletResult& OutOutlet, FString& OutError)
{
    if (const auto* Cached=BucketOutlets.Find(InAnchor)) { OutOutlet=*Cached; OutError.Reset(); return true; }
    if (!bBucketOutletActive)
    {
        FIntPoint Minimum,Maximum;
        if (!RoutingGrid.TryGetFineBucketBounds(InAnchor,Minimum,Maximum))
        { OutError=TEXT("Invalid runoff fine bucket bounds"); return false; }
        if (!BucketOutlet.Initialize(InAnchor,Minimum,Maximum,OutError)) return false;
        BucketOutletSeed=InAnchor; bBucketOutletActive=true;
    }
    if (BucketOutletSeed!=InAnchor) { OutError=TEXT("Yielded: another local runoff outlet"); return false; }
    auto Reader=[&](const FIntPoint& Cell,FVoxelHydrologySectionSample& Sample,FString& Error)
    {
        FVoxelHydrologyReceivingVoxel Fine;
        const auto Read=ReadFineWater(Cell,InLookup,Fine,Error);
        if (Read!=EVoxelHydrologyClosureRead::Ready) return Read;
        Sample.OriginalGround=Fine.OriginalGroundZ; Sample.ExistingWaterZ=Fine.WaterZ; Sample.ExistingBodyId=Fine.BodyId;
        return EVoxelHydrologyClosureRead::Ready;
    };
    const auto Step=BucketOutlet.Advance(Reader,256,OutError,ActiveCancel);
    if (Step!=EVoxelHydrologySharedSearchStep::Ready)
    {
        if (Step==EVoxelHydrologySharedSearchStep::Yielded) OutError=TEXT("Yielded: local runoff outlet certificate");
        else if (Step==EVoxelHydrologySharedSearchStep::Canceled) OutError=TEXT("Canceled");
        else if (Step==EVoxelHydrologySharedSearchStep::CapacityBlocked && OutError.IsEmpty()) OutError=TEXT("CapacityBlocked: local runoff certificate");
        return false;
    }
    if (BucketOutlets.Num()==BucketOutletCapacity)
    {
        if (BucketOutletCapacity==MAX_int32) { OutError=TEXT("Oversized: local outlet cache"); return false; }
        const int32 Capacity=HydrologyComponentCapacity(BucketOutletCapacity+1,BucketOutletCapacity);
        if (!VoxelGenerationAllocation::RetainMap(BucketOutlets,Capacity,OutError)) return false;
        BucketOutletCapacity=Capacity;
    }
    OutOutlet=BucketOutlet.GetResult(); BucketOutlets.Add(InAnchor,OutOutlet);
    BucketOutlet.FreeBacking(); bBucketOutletActive=false; OutError.Reset(); return true;
}

bool FVoxelHydrologyComponentBuild::ReadRunoffNode(const FIntPoint& InCell, FTileLookup InLookup,
    FVoxelHydrologyComponentNode& OutNode, FString& OutError)
{
    FVoxelHydrologyComponentNode Raw;
    if (!ReadRoutingFacts(InCell,InLookup,Raw,OutError)) return false;
    if (!RoutingGrid.IsGridNode(InCell))
    {
        if (!Raw.bReceiver) { OutError = TEXT("HydrologyComponentConflict: nonlattice dry runoff node"); return false; }
        OutNode = Raw; return true;
    }
    auto Apply = [&](const FCanonicalDrainage& Value)
    {
        Raw.bHasParent = Value.bHasParent; Raw.ParentCell = Value.ParentCell; Raw.bParentRunoffTerminal=Value.bParentRunoffTerminal;
        Raw.RoutePoint.WaterZ = Value.SpillZ; OutNode = Raw; OutError.Reset();
    };
    if (const auto* Cached = CanonicalDrainage.Find(InCell)) { Apply(*Cached); return true; }
    const int32 Spacing = RoutingGrid.GetSpacing();
    if (!bSharedSearchActive)
    {
        const int32 Minimum = int32(-VoxelGeneration::FloorDivideWide(-VoxelGeneration::WorldCellMin,Spacing));
        const int32 Maximum = int32(VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive-1,Spacing));
        const FIntPoint Index(VoxelGeneration::FloorDivide(InCell.X,Spacing),VoxelGeneration::FloorDivide(InCell.Y,Spacing));
        if (!SharedSearch.Initialize(Index,FIntPoint(Minimum,Minimum),FIntPoint(Maximum,Maximum),OutError)) return false;
        bSharedSearchActive = true; CertificateCopyIndex = 0;
    }
    auto Reader = [&](const FIntPoint& Index, FVoxelHydrologySharedCell& Value, FString& Error)
    {
        const FIntPoint World(int32(int64(Index.X)*Spacing),int32(int64(Index.Y)*Spacing));
        FVoxelHydrologyReceivingVoxel Fine;
        const auto Read = ReadFineWater(World,InLookup,Fine,Error);
        if (Read != EVoxelHydrologyClosureRead::Ready) return Read;
        Value = FVoxelHydrologySharedCell(); Value.Cell = Index; Value.GroundZ = Fine.OriginalGroundZ;
        Value.ReceiverWaterZ = Fine.WaterZ; Value.ReceiverId = Fine.BodyId;
        if (!Value.IsReceiver())
        {
            FVoxelHydrologyBucketOutletResult Outlet;
            if (!ReadBucketOutlet(World,InLookup,Outlet,Error))
                return Error.IsEmpty() || Error.StartsWith(TEXT("Yielded")) ? EVoxelHydrologyClosureRead::Pending : EVoxelHydrologyClosureRead::Failed;
            Value.bHasLocalOutlet=Outlet.bHasOutlet; Value.LocalOutletCell=Outlet.Terminal;
            Value.LocalOutletBarrier=Outlet.Barrier; Value.LocalOutletWaterZ=Outlet.WaterZ;
            Value.LocalOutletId=Outlet.BodyId; Value.LocalOutletWitness=Outlet.Witness;
        }
        return EVoxelHydrologyClosureRead::Ready;
    };
    const auto Step = SharedSearch.Advance(Reader,256,OutError,ActiveCancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready)
    {
        if (Step == EVoxelHydrologySharedSearchStep::Yielded) OutError = TEXT("Yielded: routing runoff certificate");
        else if (Step == EVoxelHydrologySharedSearchStep::NoReceiver)
            OutError = TEXT("HydrologyComponentConflict: no physical receiver in routing runoff domain");
        return false;
    }
    const auto& Cells = SharedSearch.GetCells();
    int32 Work = 256;
    while (CertificateCopyIndex < Cells.Num() && Work > 0)
    {
        --Work;
        if (ActiveCancel && ActiveCancel->Load()) { OutError = TEXT("Canceled"); return false; }
        const auto& Cell = Cells[CertificateCopyIndex];
        if (Cell.bIncluded)
        {
            const FIntPoint World(int32(int64(Cell.Cell.X)*Spacing),int32(int64(Cell.Cell.Y)*Spacing));
            FCanonicalDrainage Value;
            Value.SpillZ = Cell.SpillZ; Value.PlateauDistance = Cell.PlateauDistance;
            Value.bHasParent = Cell.Parent != INDEX_NONE || Cell.bParentIsLocalOutlet;
            Value.bParentRunoffTerminal=Cell.bParentIsLocalOutlet;
            if (Cell.bParentIsLocalOutlet) Value.ParentCell=Cell.LocalOutletCell;
            else if (Value.bHasParent)
            {
                const auto& Parent = Cells[Cell.Parent].Cell;
                Value.ParentCell = FIntPoint(int32(int64(Parent.X)*Spacing),int32(int64(Parent.Y)*Spacing));
            }
            if (const auto* Existing = CanonicalDrainage.Find(World))
            {
                if (Existing->SpillZ != Value.SpillZ || Existing->PlateauDistance != Value.PlateauDistance ||
                    Existing->bHasParent != Value.bHasParent || Existing->bParentRunoffTerminal!=Value.bParentRunoffTerminal || (Value.bHasParent && Existing->ParentCell != Value.ParentCell))
                { OutError = TEXT("HydrologyComponentConflict: routing runoff certificates disagree"); return false; }
            }
            else
            {
                if (CanonicalDrainage.Num() == CanonicalCapacity)
                {
                    if (CanonicalCapacity == MAX_int32) { OutError = TEXT("Oversized: routing runoff cache"); return false; }
                    const int32 Capacity = HydrologyComponentCapacity(CanonicalCapacity+1,CanonicalCapacity);
                    if (!VoxelGenerationAllocation::RetainMap(CanonicalDrainage,Capacity,OutError)) return false;
                    CanonicalCapacity = Capacity;
                }
                CanonicalDrainage.Add(World,Value);
            }
        }
        ++CertificateCopyIndex;
    }
    if (CertificateCopyIndex < Cells.Num()) { OutError = TEXT("Yielded: routing runoff publication"); return false; }
    SharedSearch.FreeBacking(); bSharedSearchActive = false;
    if (const auto* Cached = CanonicalDrainage.Find(InCell)) { Apply(*Cached); return true; }
    OutError = TEXT("Yielded: another routing runoff seed"); return false;
}

bool FVoxelHydrologyComponentBuild::ReadNode(const FIntPoint& InCell, FTileLookup InLookup,
    FVoxelHydrologyComponentNode& OutNode, FString& OutError, bool InRunoffTerminal)
{
    if (InRunoffTerminal)
    {
        FVoxelHydrologyReceivingVoxel Fine;
        if (ReadFineWater(InCell,InLookup,Fine,OutError)!=EVoxelHydrologyClosureRead::Ready) return false;
        if (Fine.WaterZ==MIN_int32 || Fine.OriginalGroundZ>=Fine.WaterZ)
        { OutError=TEXT("HydrologyComponentConflict: runoff terminal is not genuinely wet"); return false; }
        OutNode=FVoxelHydrologyComponentNode(); OutNode.Cell=InCell; OutNode.bRunoffTerminal=true;
        OutNode.bReceiver=true; OutNode.ReceiverWaterZ=Fine.WaterZ; OutNode.ReceiverId=Fine.BodyId;
        OutNode.GroundZ=Fine.OriginalGroundZ; OutNode.RoutePoint.Position=InCell; OutNode.RoutePoint.WaterZ=Fine.WaterZ;
        OutError.Reset(); return true;
    }
    FVoxelHydrologyComponentNode Raw;
    if (!ReadRoutingFacts(InCell,InLookup,Raw,OutError)) return false;
    auto Reader = [&](const FIntPoint& Position, FVoxelHydrologySectionSample& Sample, FString& Error)
    {
        FVoxelHydrologyReceivingVoxel Fine;
        const auto Status = ReadFineWater(Position,InLookup,Fine,Error);
        if (Status != EVoxelHydrologyClosureRead::Ready) return Status;
        Sample.OriginalGround = Fine.OriginalGroundZ;
        Sample.ExistingWaterZ = Fine.WaterZ; Sample.ExistingBodyId = Fine.BodyId;
        return EVoxelHydrologyClosureRead::Ready;
    };
    const auto Step = WidthRouting.Advance(InCell,Reader,256,OutError,ActiveCancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready)
    {
        if (Step == EVoxelHydrologySharedSearchStep::Yielded) OutError = TEXT("Yielded: width-aware shared routing");
        else if (Step == EVoxelHydrologySharedSearchStep::CapacityBlocked && OutError.IsEmpty()) OutError = TEXT("CapacityBlocked: width-aware routing");
        else if (Step == EVoxelHydrologySharedSearchStep::Canceled) OutError = TEXT("Canceled");
        return false;
    }
    const auto* Label = WidthRouting.Find(InCell);
    if (!Label) { OutError = TEXT("Invalid missing width label"); return false; }
    if (!Label->bEligible)
    {


        return ReadRunoffNode(InCell,InLookup,OutNode,OutError);
    }
    if (Label->W < -32768 || Label->W > 32766)
    { OutError = TEXT("Invalid certified channel water height"); return false; }
    Raw.bChannelEligible = true; Raw.bHasParent = Label->bHasParent;
    Raw.ParentCell = Label->Parent; Raw.RoutePoint.WaterZ = int32(Label->W);
    Raw.bReceiver = !Label->bHasParent;
    Raw.ReceiverWaterZ = MIN_int32; Raw.ReceiverId = FVoxelStableId();
    if (Raw.bReceiver)
    {
        const auto* Certificate = WidthRouting.FindNode(InCell);
        if (!Certificate || Certificate->Interval.FixedWaterZ == MIN_int32)
        { OutError = TEXT("HydrologyComponentConflict: channel root lacks real wet connector"); return false; }
        Raw.ReceiverWaterZ = Certificate->Interval.FixedWaterZ;
        Raw.ReceiverId = Certificate->Interval.FixedBodyId;
    }
    OutNode = Raw; OutError.Reset(); return true;
}

bool FVoxelHydrologyComponentBuild::AddNode(const FVoxelHydrologyComponentNode& InNode,
    int32& OutIndex, FString& OutError)
{
    if (const int32* Existing = NodeByCell.Find(FIntVector(InNode.Cell.X,InNode.Cell.Y,InNode.bRunoffTerminal?1:0)))
    {
        OutIndex = *Existing;
        return true;
    }
    if (Nodes.Num() == Nodes.Max() && !VoxelGenerationAllocation::RetainArray(Nodes,
        HydrologyComponentCapacity(Nodes.Num() + 1, Nodes.Max()), OutError))
    {
        return false;
    }
    if (NodeByCell.Num() == MapCapacity)
    {
        const int32 Capacity = HydrologyComponentCapacity(MapCapacity + 1, MapCapacity);
        if (!VoxelGenerationAllocation::RetainMap(NodeByCell, Capacity, OutError))
        {
            return false;
        }
        MapCapacity = Capacity;
    }
    OutIndex = Nodes.Add(InNode);
    NodeByCell.Add(FIntVector(InNode.Cell.X,InNode.Cell.Y,InNode.bRunoffTerminal?1:0), OutIndex);
    return true;
}

EVoxelHydrologyComponentStep FVoxelHydrologyComponentBuild::Advance(FTileLookup InLookup,
    int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel,
    FVoxelGenerationPlanCache* InFineCache,
    const TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>& InConfig)
{
    if ((InFineCache != nullptr) != InConfig.IsValid())
    {
        OutError = TEXT("Invalid fine water cache/config context");
        return EVoxelHydrologyComponentStep::Failed;
    }
    check(!ActiveFineCache && !ActiveConfig);
    ActiveFineCache = InFineCache;
    ActiveConfig = &InConfig;
    ON_SCOPE_EXIT
    {
        ActiveFineCache = nullptr;
        ActiveConfig = nullptr;
    };
    ActiveCancel = InCancel;
    ON_SCOPE_EXIT
    {
        static TAtomic<uint64> NextDiagnosticMilliseconds(0);
        const uint64 Now = uint64(FPlatformTime::Seconds() * 1000.0);
        uint64 Expected = NextDiagnosticMilliseconds.Load();
        if (Now >= Expected && NextDiagnosticMilliseconds.CompareExchange(Expected, Now + 5000))
        {
            UE_LOG(LogTemp, Display,
                TEXT("Voxel hydrology component progress: seed=(%d,%d) mapped=%d initialized=%d nodes=%d expanded=%d filtered=%d geometry=%d profiles=%d ready=%d fineActive=%d fineBytes=%llu fineCached=%d lakeActive=%d lakeBytes=%llu lakeRoots=%d widthBytes=%llu sharedActive=%d sharedBytes=%llu totalBytes=%llu error=%s"),
                SeedCell.X, SeedCell.Y, bSeedMapped, bSeedInitialized, Nodes.Num(), ExpandIndex,
                bNodesFiltered, bGeometryBuilt, bProfilesSolved, bReady, bFineWaterActive,
                FineWaterQuery.GetAllocatedBytes(), FineWaterCache.Num(), bLakeCertificateActive,
                LakeCertificate.GetAllocatedBytes(), CertifiedFineRoots.Num(), WidthRouting.GetAllocatedBytes(),
                bSharedSearchActive, SharedSearch.GetAllocatedBytes(), GetAllocatedBytes(), *OutError);
            if (bFineWaterActive)
            {
                UE_LOG(LogTemp, Display,
                    TEXT("Voxel fine search progress: cells=%d threshold=%d stage=%d"),
                    FineWaterQuery.GetSearchCellCount(), FineWaterQuery.GetSearchThreshold(),
                    FineWaterQuery.GetProgressStage());
            }
        }
    };
    auto Failure = [&OutError]()
    {
        if (OutError == TEXT("Canceled"))
        {
            return EVoxelHydrologyComponentStep::Canceled;
        }
        if (OutError.StartsWith(TEXT("Yielded")) || OutError.StartsWith(TEXT("DependencyNotReady")))
        {
            return EVoxelHydrologyComponentStep::Yielded;
        }
        if (OutError.StartsWith(TEXT("CapacityBlocked")))
        {
            return EVoxelHydrologyComponentStep::CapacityBlocked;
        }
        if (OutError.StartsWith(TEXT("RiverProfileConflict")) || OutError.StartsWith(TEXT("HydrologyComponentConflict")))
        {
            return EVoxelHydrologyComponentStep::Conflict;
        }
        return EVoxelHydrologyComponentStep::Failed;
    };
    if (InCancel && InCancel->Load())
    {
        OutError = TEXT("Canceled");
        return EVoxelHydrologyComponentStep::Canceled;
    }
    if (bReady)
    {
        OutError.Reset();
        return EVoxelHydrologyComponentStep::Ready;
    }
    MissingTiles.Reset();
    OutError.Reset();
    if (InWorkBudget <= 0)
    {
        return EVoxelHydrologyComponentStep::Yielded;
    }
    if (InCancel && InCancel->Load())
    {
        OutError = TEXT("Canceled");
        return EVoxelHydrologyComponentStep::Canceled;
    }
    if (!bSeedMapped)
    {
        if (!InitializeRouting(OutError)) return Failure();
        FIntPoint WorldSeed;
        if (!RoutingGrid.TryMapCoarseCell(SeedCell,WorldSeed))
        { OutError = TEXT("Invalid coarse seed routing ownership"); return Failure(); }
        SeedCell = WorldSeed; bSeedMapped = true;
    }
    if (!bSeedInitialized)
    {
        FVoxelHydrologyComponentNode Seed;
        if (!ReadNode(SeedCell, InLookup, Seed, OutError))
        {
            return OutError.IsEmpty() && !MissingTiles.IsEmpty() ? EVoxelHydrologyComponentStep::NeedTiles : Failure();
        }
        int32 SeedIndex = INDEX_NONE;
        if (!AddNode(Seed, SeedIndex, OutError))
        {
            return Failure();
        }
        bSeedInitialized=true;
    }
    static const FIntPoint Offsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
    while (ExpandIndex < Nodes.Num() && InWorkBudget > 0)
    {
        --InWorkBudget;
        if (InCancel && InCancel->Load())
        {
            OutError = TEXT("Canceled");
            return EVoxelHydrologyComponentStep::Canceled;
        }
        const FVoxelHydrologyComponentNode Current = Nodes[ExpandIndex];
        FVoxelHydrologyComponentNode Parent;
        FVoxelHydrologyComponentNode Upstream[4];
        bool bAddUpstream[4] = {false, false, false, false};
        bool bComplete = true;
        if (Current.bHasParent && !ReadNode(Current.ParentCell, InLookup, Parent, OutError, Current.bParentRunoffTerminal))
        {
            if (!OutError.IsEmpty())
            {
                return Failure();
            }
            return EVoxelHydrologyComponentStep::NeedTiles;
        }
        FIntPoint Candidates[4];
        int32 CandidateCount=0;
        if (Current.bRunoffTerminal)
        {
            FIntPoint Owner;
            if (!RoutingGrid.TryGetBucketOwner(Current.Cell,Owner))
            { OutError=TEXT("Invalid runoff terminal bucket owner"); return Failure(); }
            Candidates[CandidateCount++]=Owner;
        }
        else CandidateCount=RoutingGrid.GetReverseCandidates(Current.Cell,Candidates);
        for (int32 Face = 0; Face < CandidateCount; ++Face)
        {
            const FIntPoint Neighbor = Candidates[Face];
            if (!ReadNode(Neighbor, InLookup, Upstream[Face], OutError))
            {
                if (!OutError.IsEmpty())
                {
                    return Failure();
                }
                return EVoxelHydrologyComponentStep::NeedTiles;
            }
            bAddUpstream[Face] = Upstream[Face].bHasParent &&
                Upstream[Face].ParentCell == Current.Cell && Upstream[Face].bParentRunoffTerminal==Current.bRunoffTerminal;
        }
        if (!bComplete)
        {
            return EVoxelHydrologyComponentStep::NeedTiles;
        }
        if (Current.bHasParent)
        {
            int32 ParentIndex = INDEX_NONE;
            if (!AddNode(Parent, ParentIndex, OutError))
            {
                return Failure();
            }
            Nodes[ExpandIndex].Parent = ParentIndex;
        }
        for (int32 Face = 0; Face < 4; ++Face)
        {
            if (bAddUpstream[Face])
            {
                int32 UpstreamIndex = INDEX_NONE;
                if (!AddNode(Upstream[Face], UpstreamIndex, OutError))
                {
                    return Failure();
                }
            }
        }
        Nodes[ExpandIndex].bExpanded = true;
        ++ExpandIndex;
    }
    if (ExpandIndex < Nodes.Num())
    {
        return EVoxelHydrologyComponentStep::Yielded;
    }
    if (!bNodesFiltered)
    {
        if (!PrepareRiverNodes(OutError, InCancel))
        {
            return Failure();
        }
        bNodesFiltered = true;
    }


    if (!bGeometryBuilt)
    {
        if (!BuildGeometry(InLookup, OutError, InCancel))
        {
            return Failure();
        }
        bGeometryBuilt = true;
        return EVoxelHydrologyComponentStep::Yielded;
    }
    if (!ResolveProfiles(InLookup, OutError, InCancel))
    {
        return OutError.IsEmpty() && !MissingTiles.IsEmpty() ? EVoxelHydrologyComponentStep::NeedTiles : Failure();
    }
    bReady = true;
    OutError.Reset();
    return EVoxelHydrologyComponentStep::Ready;
}

const TArray<FVoxelHydrologyRegionKey>& FVoxelHydrologyComponentBuild::GetMissingTiles() const
{
    return MissingTiles;
}

uint64 FVoxelHydrologyComponentBuild::GetAllocatedBytes() const
{
    return VoxelGenerationAllocation::AllocatedBytes(Nodes) +
        VoxelGenerationAllocation::AllocatedBytes(NodeByCell) +
        VoxelGenerationAllocation::AllocatedBytes(MissingTiles) +
        BucketOutlet.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(BucketOutlets) +
        ProfileClosure.GetAllocatedBytes() + SharedSearch.GetAllocatedBytes() + WidthRouting.GetAllocatedBytes() + Decoration.GetAllocatedBytes() +
        LakeCertificate.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(CertifiedFineRoots) +
        FineWaterQuery.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(FineWaterCache) +
        VoxelGenerationAllocation::AllocatedBytes(RawGroundCache) + VoxelGenerationAllocation::AllocatedBytes(RawGroundRing) +
        VoxelGenerationAllocation::AllocatedBytes(CertifiedReceivingOrder) + VoxelGenerationAllocation::AllocatedBytes(PrepareChildren) + VoxelGenerationAllocation::AllocatedBytes(PrepareQueue) +
        VoxelGenerationAllocation::AllocatedBytes(PrepareRemap) + VoxelGenerationAllocation::AllocatedBytes(PrepareWinners) +
        VoxelGenerationAllocation::AllocatedBytes(PrepareActive) + VoxelGenerationAllocation::AllocatedBytes(PrepareQualifiedChild) +
        VoxelGenerationAllocation::AllocatedBytes(PrepareBestSource) + VoxelGenerationAllocation::AllocatedBytes(PrepareSelected) +
        VoxelGenerationAllocation::AllocatedBytes(CanonicalDrainage) + VoxelGenerationAllocation::AllocatedBytes(RoutingRain) + Result.GetAllocatedBytes();
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

uint64 FVoxelHydrologyRiverComponent::GetAllocatedBytes() const
{
    return Plan.GetAllocatedBytes() + VoxelGenerationAllocation::AllocatedBytes(DrainageCells) +
        VoxelGenerationAllocation::AllocatedBytes(LakeWaterByVoxel) +
        VoxelGenerationAllocation::AllocatedBytes(ReceivingWaterByVoxel) + VoxelGenerationAllocation::AllocatedBytes(RunoffOutlets);
}

bool FVoxelHydrologyRiverComponent::HasEquivalentOutput(const FVoxelHydrologyRiverComponent& InOther) const
{
    if (Key != InOther.Key || Fingerprint != InOther.Fingerprint ||
        DrainageCells != InOther.DrainageCells || Plan.Grid.CellSize != InOther.Plan.Grid.CellSize ||
        Plan.bCompleteRiverIndex != InOther.Plan.bCompleteRiverIndex ||
        Plan.Rivers.Num() != InOther.Plan.Rivers.Num() ||
        Plan.Waterfalls.Num() != InOther.Plan.Waterfalls.Num() ||
        LakeWaterByVoxel.Num() != InOther.LakeWaterByVoxel.Num() ||
        ReceivingWaterByVoxel.Num() != InOther.ReceivingWaterByVoxel.Num() || RunoffOutlets.Num()!=InOther.RunoffOutlets.Num() || RunoffFingerprint!=InOther.RunoffFingerprint || TotalRainWeight!=InOther.TotalRainWeight || ReceivedRainWeight!=InOther.ReceivedRainWeight) return false;
    for (const auto& Pair:RunoffOutlets)
    {
        const auto* Other=InOther.RunoffOutlets.Find(Pair.Key); const auto& A=Pair.Value;
        if (!Other || A.bHasOutlet!=Other->bHasOutlet || A.Terminal!=Other->Terminal || A.WaterZ!=Other->WaterZ ||
            !(A.BodyId==Other->BodyId) || A.Barrier!=Other->Barrier || A.PathLength!=Other->PathLength || A.Witness!=Other->Witness) return false;
    }
    auto SameWater = [](const FVoxelHydrologyReceivingVoxel& A, const FVoxelHydrologyReceivingVoxel& B)
    { return A.WaterZ == B.WaterZ && A.BodyId == B.BodyId && A.bOcean == B.bOcean; };
    if (!SameWater(ExpandedReceivingWater, InOther.ExpandedReceivingWater)) return false;
    for (const auto& Pair : LakeWaterByVoxel)
    {
        const int32* Water = InOther.LakeWaterByVoxel.Find(Pair.Key);
        if (!Water || *Water != Pair.Value) return false;
    }
    for (const auto& Pair : ReceivingWaterByVoxel)
    {
        const auto* Water = InOther.ReceivingWaterByVoxel.Find(Pair.Key);
        if (!Water || !SameWater(*Water, Pair.Value)) return false;
    }
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
    const FIntPoint Cell(InWorldX, InWorldY);
    auto SetReceivingInfluence = [&](const FVoxelHydrologyReceivingVoxel& InWater)
    {

        if (InWater.WaterZ == MIN_int32 || InOriginalGround >= InWater.WaterZ) return false;
        FVoxelHydrologyInfluence Influence;
        Influence.SurfaceWaterZ = InWater.WaterZ;
        Influence.GroundOverrideZ = InOriginalGround;
        Influence.bOcean = InWater.bOcean;
        Influence.bLake = !InWater.bOcean;
        OutInfluence = Influence;
        return true;
    };
    if (const auto* Water = ReceivingWaterByVoxel.Find(Cell))
    {
        if (SetReceivingInfluence(*Water)) return true;
    }
    if (const int32* Water = LakeWaterByVoxel.Find(Cell))
    {
        FVoxelHydrologyReceivingVoxel Expanded = ExpandedReceivingWater;
        Expanded.WaterZ = *Water;
        if (SetReceivingInfluence(Expanded)) return true;
    }
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

bool FVoxelHydrologyComponentBuild::PrepareRiverNodes(FString& OutError, const TAtomic<bool>* InCancel)
{
    const auto& Settings = Recipe->Settings;
    const uint64 Threshold = FMath::Max<uint64>(1,Settings.RiverSourceAccumulation);
    auto SourceId = [&](int32 Index) { return VoxelGeneration::MakeStableId(Settings.Seed,
        FIntVector(Nodes[Index].Cell.X,Nodes[Index].Cell.Y,0),0x5249564552535243ull,0); };
    auto Fail = [&](const TCHAR* Message) { OutError = Message; return false; };
    int32 Work = 256;
    while (Work > 0)
    {
        --Work;
        if (InCancel && InCancel->Load()) return Fail(TEXT("Canceled"));
        if (PrepareStage == 0)
        {
            if (PrepareCursor < Nodes.Num())
            {
                const auto& Node = Nodes[PrepareCursor];
                if (Node.bParentRunoffTerminal && !Result.RunoffOutlets.Contains(Node.Cell))
                {
                    const auto* Outlet=BucketOutlets.Find(Node.Cell);
                    if (!Outlet || !Outlet->bHasOutlet || Outlet->Terminal!=Node.ParentCell) return Fail(TEXT("Missing final typed runoff outlet"));
                    if (Result.RunoffOutlets.Num()==PublishedOutletCapacity)
                    {
                        if (PublishedOutletCapacity==MAX_int32) return Fail(TEXT("Oversized: published runoff outlets"));
                        const int32 Capacity=HydrologyComponentCapacity(PublishedOutletCapacity+1,PublishedOutletCapacity);
                        if (!VoxelGenerationAllocation::RetainMap(Result.RunoffOutlets,Capacity,OutError)) return false;
                        PublishedOutletCapacity=Capacity;
                    }
                    Result.RunoffOutlets.Add(Node.Cell,*Outlet);
                    uint64 H=VoxelGeneration::Mix((uint64(uint32(Node.Cell.X))<<32)|uint32(Node.Cell.Y));
                    H=VoxelGeneration::Mix(H^((uint64(uint32(Outlet->Terminal.X))<<32)|uint32(Outlet->Terminal.Y)));
                    H=VoxelGeneration::Mix(H^Outlet->BodyId.High^VoxelGeneration::Mix(Outlet->BodyId.Low));
                    H=VoxelGeneration::Mix(H^uint32(Outlet->WaterZ)^VoxelGeneration::Mix(uint32(Outlet->Barrier))^Outlet->Witness^uint32(Outlet->PathLength));
                    Result.RunoffFingerprint^=H;
                }
                ++PrepareCursor;
                if (Node.bRunoffTerminal || !RoutingGrid.IsGridNode(Node.Cell)) continue;
                FIntPoint Minimum,Maximum;
                if (!RoutingGrid.TryGetCoarseBounds(Node.Cell,Minimum,Maximum)) return Fail(TEXT("Invalid rainfall ownership bounds"));
                PrepareCoarseCount += uint64(int64(Maximum.X)-Minimum.X+1)*uint64(int64(Maximum.Y)-Minimum.Y+1);
                if (PrepareCoarseCount > MAX_int32) return Fail(TEXT("Oversized: coarse drainage ownership"));
                continue;
            }
            if (!VoxelGenerationAllocation::RetainArray(PrepareChildren,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareQueue,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareRemap,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareActive,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareQualifiedChild,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainArray(PrepareWinners,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainMap(PrepareBestSource,Nodes.Num(),OutError) ||
                !VoxelGenerationAllocation::RetainArray(Result.DrainageCells,int32(PrepareCoarseCount),OutError)) return false;
            PrepareCursor = 0; PrepareStage = 1;
        }
        else if (PrepareStage == 1)
        {
            if (PrepareCursor < Nodes.Num())
            {
                PrepareChildren.Add(0); PrepareRemap.Add(INDEX_NONE); PrepareActive.Add(0); PrepareQualifiedChild.Add(0);
                ++PrepareCursor; continue;
            }
            PrepareCursor = 0; PrepareStage = 2;
        }
        else if (PrepareStage == 2)
        {
            if (PrepareCursor < Nodes.Num())
            {
                auto& Node = Nodes[PrepareCursor++]; Node.RoutePoint.Accumulation = Node.RainWeight; Node.bSelected = false;
                if (MAX_uint64-Result.TotalRainWeight<Node.RainWeight) return Fail(TEXT("Oversized: total shared rain"));
                Result.TotalRainWeight+=Node.RainWeight;

                if (Node.bChannelEligible)
                {
                    const auto* Certificate=WidthRouting.FindNode(Node.Cell);
                    const auto* Minimum=WidthRouting.Find(Node.Cell);
                    if (!Certificate || !Minimum || !Minimum->bEligible) return Fail(TEXT("Missing actual-water node certificate"));
                    const int64 Water=Node.bReceiver ? Node.ReceiverWaterZ : Certificate->Interval.UpperWaterZ;
                    if (Water<Minimum->W || Water>32766 || Water< -32768) return Fail(TEXT("Invalid actual-water initial cap"));
                    Node.RoutePoint.WaterZ=int32(Water);
                }
                if (Node.Parent != INDEX_NONE)
                {
                    if (!Nodes.IsValidIndex(Node.Parent)) return Fail(TEXT("HydrologyComponentConflict: incomplete shared parent"));
                    ++PrepareChildren[Node.Parent];
                }
                else if (!Node.bReceiver) return Fail(TEXT("HydrologyComponentConflict: dry runoff terminal"));
                continue;
            }
            PrepareCursor = 0; PrepareStage = 3;
        }
        else if (PrepareStage == 3)
        {
            if (PrepareCursor < Nodes.Num())
            { if (PrepareChildren[PrepareCursor] == 0) PrepareQueue.Add(PrepareCursor); ++PrepareCursor; continue; }
            PrepareCursor = 0; PrepareStage = 4;
        }
        else if (PrepareStage == 4)
        {
            if (PrepareCursor < PrepareQueue.Num())
            {
                const int32 Index = PrepareQueue[PrepareCursor++], Parent = Nodes[Index].Parent;
                if (Parent != INDEX_NONE)
                {
                    const uint64 Rain = Nodes[Index].RoutePoint.Accumulation;
                    uint64& Down = Nodes[Parent].RoutePoint.Accumulation;
                    if (MAX_uint64-Down<Rain) return Fail(TEXT("Oversized: shared rainfall accumulation"));
                    Down += Rain;
                    if (Nodes[Index].bChannelEligible)
                    {
                        const auto* Port=WidthRouting.FindSelectedPort(Nodes[Index].Cell);
                        const auto* Minimum=WidthRouting.Find(Nodes[Parent].Cell);
                        if (!Port || !Minimum || !Minimum->bEligible) return Fail(TEXT("Missing actual-water edge certificate"));
                        const int64 Cap=FMath::Min<int64>(Nodes[Index].RoutePoint.WaterZ,Port->GetResult().Transfer.B);
                        const int64 Water=FMath::Min<int64>(Nodes[Parent].RoutePoint.WaterZ,Cap);
                        if (Water<Minimum->W || (Nodes[Parent].bReceiver && Water!=Nodes[Parent].ReceiverWaterZ))
                            return Fail(TEXT("HydrologyComponentConflict: actual-water cap below feasible receiver path"));
                        Nodes[Parent].RoutePoint.WaterZ=int32(Water);
                    }
                    if (--PrepareChildren[Parent] == 0) PrepareQueue.Add(Parent);
                }
                else
                {
                    const uint64 Rain=Nodes[Index].RoutePoint.Accumulation;
                    if (MAX_uint64-Result.ReceivedRainWeight<Rain) return Fail(TEXT("Oversized: total received rain"));
                    Result.ReceivedRainWeight+=Rain;
                }
                continue;
            }
            if (PrepareQueue.Num() != Nodes.Num()) return Fail(TEXT("HydrologyComponentConflict: hybrid parent cycle"));
            if (Result.TotalRainWeight!=Result.ReceivedRainWeight) return Fail(TEXT("HydrologyComponentConflict: shared rainfall was not conserved"));
            PrepareCursor = 0; PrepareStage = 5;
        }
        else if (PrepareStage == 5)
        {
            if (PrepareCursor < Nodes.Num())
            {
                auto& Node = Nodes[PrepareCursor++];
                if (Node.bChannelEligible && Node.RoutePoint.Accumulation >= Threshold && Node.Parent != INDEX_NONE)
                    PrepareQualifiedChild[Node.Parent] = 1;
                const uint64 Relative = FMath::Max<uint64>(1,Node.RoutePoint.Accumulation/Threshold);
                const int32 Growth = FMath::Clamp(int32(FMath::FloorLog2(uint32(FMath::Min<uint64>(Relative,MAX_uint32)))),0,Settings.RiverMaxGrowthLevels);
                Node.DesiredWaterHalfWidth = Settings.RiverBaseHalfWidth+Growth*Settings.RiverWidthGrowthPerLevel;
                Node.DesiredDepth = Settings.RiverBaseDepth+Growth*Settings.RiverDepthGrowthPerLevel;
                Node.RoutePoint.WaterHalfWidth = RoutingShape.WaterHalfWidth;
                Node.RoutePoint.BedHalfWidth = RoutingShape.BedHalfWidth; Node.RoutePoint.Depth = RoutingShape.Depth;
                continue;
            }
            PrepareCursor = 0; PrepareStage = 6;
        }
        else if (PrepareStage == 6)
        {
            if (PrepareCursor < Nodes.Num())
            {
                const int32 Index = PrepareCursor++; const auto& Node = Nodes[Index];
                if (!Node.bChannelEligible || Node.bReceiver || !Node.bHasParent || Node.RoutePoint.Accumulation<Threshold || PrepareQualifiedChild[Index]) continue;
                const int64 Span = int64(Settings.RiverSourceSpacing)*Settings.HydrologyCellSize;
                if (Span <= 0) return Fail(TEXT("Invalid source bucket span"));
                const FIntPoint Bucket(int32(VoxelGeneration::FloorDivideWide(Node.Cell.X,Span)),int32(VoxelGeneration::FloorDivideWide(Node.Cell.Y,Span)));
                if (const int32* Slot = PrepareBestSource.Find(Bucket))
                {
                    const int32 Previous = PrepareWinners[*Slot];
                    if (Node.RoutePoint.Accumulation>Nodes[Previous].RoutePoint.Accumulation ||
                        (Node.RoutePoint.Accumulation==Nodes[Previous].RoutePoint.Accumulation && SourceId(Index)<SourceId(Previous)))
                        PrepareWinners[*Slot] = Index;
                }
                else { const int32 WinnerSlot = PrepareWinners.Add(Index); PrepareBestSource.Add(Bucket,WinnerSlot); }
                continue;
            }
            PrepareCursor = 0; PrepareStage = 7;
        }
        else if (PrepareStage == 7)
        {
            if (PrepareCursor < PrepareWinners.Num())
            { const int32 Index = PrepareWinners[PrepareCursor++]; PrepareActive[Index] = 1; Nodes[Index].bSelected = true; continue; }
            PrepareCursor = 0; PrepareStage = 8;
        }
        else if (PrepareStage == 8)
        {
            if (PrepareCursor < PrepareQueue.Num())
            {
                const int32 Index = PrepareQueue[PrepareCursor++];
                if (Nodes[Index].Parent != INDEX_NONE) PrepareActive[Nodes[Index].Parent] |= PrepareActive[Index];
                continue;
            }
            PrepareCursor = 0; PrepareStage = 9;
        }
        else if (PrepareStage == 9)
        {
            if (PrepareCursor < Nodes.Num())
            {
                const auto& Node = Nodes[PrepareCursor];
                if (!PrepareBucketActive)
                {
                    if (PrepareActive[PrepareCursor]) ++PrepareActiveCount;
                    if (Node.bRunoffTerminal || !RoutingGrid.IsGridNode(Node.Cell)) { ++PrepareCursor; continue; }
                    if (!RoutingGrid.TryGetCoarseBounds(Node.Cell,PrepareBucketMin,PrepareBucketMax)) return Fail(TEXT("Invalid rainfall bucket during publication"));
                    PrepareBucketNext = PrepareBucketMin; PrepareBucketActive = true;
                }
                Result.DrainageCells.Add(PrepareBucketNext);
                if (PrepareBucketNext.Y == PrepareBucketMax.Y)
                {
                    PrepareBucketNext.Y = PrepareBucketMin.Y;
                    if (PrepareBucketNext.X == PrepareBucketMax.X) { PrepareBucketActive = false; ++PrepareCursor; }
                    else ++PrepareBucketNext.X;
                }
                else ++PrepareBucketNext.Y;
                continue;
            }
            if (Result.DrainageCells.IsEmpty()) return Fail(TEXT("Invalid component without coarse ownership"));
            PrepareSortIndex = Result.DrainageCells.Num()/2-1; PrepareSortEnd = Result.DrainageCells.Num()-1;
            PrepareStage = 10;
        }
        else if (PrepareStage == 10 || PrepareStage == 11)
        {
            auto Sift = [&](int32 Root,int32 End)
            {
                while (int64(Root)*2+1 <= End)
                {
                    int32 Child = Root*2+1;
                    if (Child<End && HydrologyComponentCellLess(Result.DrainageCells[Child],Result.DrainageCells[Child+1])) ++Child;
                    if (!HydrologyComponentCellLess(Result.DrainageCells[Root],Result.DrainageCells[Child])) break;
                    Swap(Result.DrainageCells[Root],Result.DrainageCells[Child]); Root = Child;
                }
            };
            if (PrepareStage == 10)
            {
                if (PrepareSortIndex >= 0) { Sift(PrepareSortIndex--,PrepareSortEnd); continue; }
                PrepareStage = 11;
            }
            else
            {
                if (PrepareSortEnd > 0)
                { Swap(Result.DrainageCells[0],Result.DrainageCells[PrepareSortEnd]); Sift(0,--PrepareSortEnd); continue; }
                Result.Key = Result.DrainageCells[0];
                if (!VoxelGenerationAllocation::RetainArray(PrepareSelected,PrepareActiveCount,OutError)) return false;
                PrepareCursor = 0; PrepareStage = 12;
            }
        }
        else if (PrepareStage == 12)
        {
            if (PrepareCursor < Nodes.Num())
            {
                const int32 Index = PrepareCursor++;
                if (PrepareActive[Index])
                {
                    if (!Nodes[Index].bChannelEligible) return Fail(TEXT("HydrologyComponentConflict: selected river leaves eligible graph"));
                    PrepareRemap[Index] = PrepareSelected.Add(Nodes[Index]);
                }
                continue;
            }
            PrepareCursor = 0; PrepareStage = 13;
        }
        else if (PrepareStage == 13)
        {
            if (PrepareCursor < PrepareSelected.Num())
            {
                auto& Node = PrepareSelected[PrepareCursor++];
                if (Node.Parent != INDEX_NONE) Node.Parent = PrepareRemap[Node.Parent];
                continue;
            }
            VoxelGenerationAllocation::FreeRetainedArray(Nodes); Nodes = MoveTemp(PrepareSelected);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareChildren); VoxelGenerationAllocation::FreeRetainedArray(PrepareQueue);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareRemap); VoxelGenerationAllocation::FreeRetainedArray(PrepareActive);
            VoxelGenerationAllocation::FreeRetainedArray(PrepareQualifiedChild); VoxelGenerationAllocation::FreeRetainedArray(PrepareWinners);
            VoxelGenerationAllocation::FreeRetainedMap(PrepareBestSource); VoxelGenerationAllocation::FreeRetainedMap(NodeByCell);
            VoxelGenerationAllocation::FreeRetainedMap(CanonicalDrainage); SharedSearch.FreeBacking();
            VoxelGenerationAllocation::FreeRetainedMap(BucketOutlets); BucketOutlet.FreeBacking(); BucketOutletCapacity=0; bBucketOutletActive=false;
            bSharedSearchActive = false; CanonicalCapacity = MapCapacity = 0; PrepareStage = 14;
        }
        else { OutError.Reset(); return true; }
    }
    OutError = TEXT("Yielded: shared rainfall and source preparation"); return false;
}

bool FVoxelHydrologyComponentBuild::CollectRegionSeeds(const FVoxelHydrologyRegionKey& InRegion,
    const FVoxelGenerationSettings& InSettings, FTileLookup InLookup,
    TArray<FIntPoint>& OutSeeds, TArray<FVoxelHydrologyRegionKey>& OutMissingTiles, FString& OutError)
{
    if (InSettings.HydrologyCellSize <= 0 || InSettings.HydrologyRegionSide <= 0 ||
        !IsHydrologyComponentRegion(InRegion, InSettings))
    {
        OutError = TEXT("Invalid hydrology component region");
        return false;
    }
    const FVoxelHydrologyPlan* BaseTile = InLookup(InRegion);
    if (!BaseTile)
    {
        if (!VoxelGenerationAllocation::RetainArray(OutMissingTiles, 1, OutError))
        {
            return false;
        }
        OutMissingTiles.Reset();
        OutMissingTiles.Add(InRegion);
        OutError.Reset();
        return false;
    }
    const int64 MaximumInfluence = int64(InSettings.RiverBaseHalfWidth) +
        int64(InSettings.RiverMaxGrowthLevels) * InSettings.RiverWidthGrowthPerLevel +
        InSettings.RiverBankWidth + FMath::Max(InSettings.RiverBankWidth + 1, InSettings.RiverShoreWidth) +
        InSettings.RiverMeanderStrength + InSettings.HydrologyCellSize;
    const int64 Margin = (MaximumInfluence + InSettings.HydrologyCellSize - 1) / InSettings.HydrologyCellSize;
    const int64 DomainMinimum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMin,
        InSettings.HydrologyCellSize);
    const int64 DomainMaximum = VoxelGeneration::FloorDivideWide(VoxelGeneration::WorldCellMaxExclusive - 1,
        InSettings.HydrologyCellSize) + 1;
    const int64 MinimumX = FMath::Max(DomainMinimum, int64(InRegion.Coordinate.X) * InSettings.HydrologyRegionSide - Margin);
    const int64 MinimumY = FMath::Max(DomainMinimum, int64(InRegion.Coordinate.Y) * InSettings.HydrologyRegionSide - Margin);
    const int64 MaximumX = FMath::Min(DomainMaximum, (int64(InRegion.Coordinate.X) + 1) * InSettings.HydrologyRegionSide + Margin);
    const int64 MaximumY = FMath::Min(DomainMaximum, (int64(InRegion.Coordinate.Y) + 1) * InSettings.HydrologyRegionSide + Margin);
    const int64 Count = (MaximumX - MinimumX) * (MaximumY - MinimumY);
    if (Count <= 0 || Count > MAX_int32)
    {
        OutError = TEXT("Oversized: hydrology component region seed count");
        return false;
    }
    if (!VoxelGenerationAllocation::RetainArray(OutSeeds, int32(Count), OutError))
    {
        return false;
    }
    OutSeeds.Reset();
    OutMissingTiles.Reset();
    auto ReadCanonicalCell = [&](const FIntPoint& Cell, const FVoxelHydrologyPlan*& Tile,
        uint32& Index) -> bool
    {
        const FVoxelHydrologyRegionKey Key{FIntPoint(
            VoxelGeneration::FloorDivide(Cell.X, InSettings.HydrologyRegionSide),
            VoxelGeneration::FloorDivide(Cell.Y, InSettings.HydrologyRegionSide))};
        Tile = InLookup(Key);
        if (!Tile)
        {
            if (!VoxelGenerationAllocation::RetainArray(OutMissingTiles, 1, OutError)) return false;
            OutMissingTiles.Add(Key);
            OutError.Reset();
            return false;
        }
        const int64 LocalX = int64(Cell.X) - Tile->Grid.WorldMinCell.X;
        const int64 LocalY = int64(Cell.Y) - Tile->Grid.WorldMinCell.Y;
        if (LocalX < 0 || LocalY < 0 || LocalX >= Tile->Grid.Width || LocalY >= Tile->Grid.Height)
        {
            OutError = TEXT("HydrologyComponentConflict: canonical seed tile omits an owned cell");
            return false;
        }
        Index = Tile->Grid.ToIndex(int32(LocalX), int32(LocalY));
        if (!Tile->Drainage.Parent.IsValidIndex(int32(Index)))
        {
            OutError = TEXT("HydrologyComponentConflict: canonical seed tile has incomplete drainage");
            return false;
        }
        return true;
    };
    static const FIntPoint NeighborOffsets[] = {FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1)};
    for (int64 CellX = MinimumX; CellX < MaximumX; ++CellX)
    {
        for (int64 CellY = MinimumY; CellY < MaximumY; ++CellY)
        {
            const FIntPoint Cell{int32(CellX), int32(CellY)};
            const FVoxelHydrologyPlan* Canonical = nullptr;
            uint32 Index = 0;
            if (!ReadCanonicalCell(Cell, Canonical, Index)) return false;


            OutSeeds.Add(FIntPoint(int32(CellX), int32(CellY)));
        }
    }
    OutError.Reset();
    return true;
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
