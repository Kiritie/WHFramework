#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"

#include "Voxel/Generation/Hydrology/VoxelRiverCorridor.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

bool FVoxelHydrologyComponentBuild::BuildGeometry(FString& OutError, const TAtomic<bool>* InCancel)
{
    auto& Plan = Result.Plan;
    if (!bCertifiedGeometryInitialized)
    {
        if (!VoxelGenerationAllocation::RetainArray(Plan.Rivers, Nodes.Num(), OutError)) return false;
        Plan.RiverSeed = Recipe->Settings.Seed;
        Plan.RiverShapeSmoothingPasses = Recipe->Settings.RiverShapeSmoothingPasses;
        Plan.RiverMeanderStrength = Recipe->Settings.RiverMeanderStrength;
        Plan.RiverMeanderFrequency = Recipe->Settings.RiverMeanderFrequency;
        Plan.RiverMeanderOctaves = Recipe->Settings.RiverMeanderOctaves;
        Plan.RiverMaxMeanderAngle = Recipe->Settings.RiverMaxMeanderAngle;
        Plan.Grid.CellSize = Recipe->Settings.HydrologyCellSize;
        Plan.bCompleteRiverIndex = true;
        Plan.bMacroAuthoritative = true;
        Plan.CoreMin = FIntPoint(int32(VoxelGeneration::WorldCellMin), int32(VoxelGeneration::WorldCellMin));
        Plan.CoreMax = FIntPoint(int32(VoxelGeneration::WorldCellMaxExclusive), int32(VoxelGeneration::WorldCellMaxExclusive));
        bCertifiedGeometryInitialized = true;
    }
    int32 Work = 256;
    while (CertifiedGeometryNode < Nodes.Num() && Work-- > 0)
    {
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
        const auto& Node = Nodes[CertifiedGeometryNode];
        if (Node.bReceiver || Node.bGhost) { ++CertifiedGeometryNode; continue; }
        if (!Nodes.IsValidIndex(Node.Parent))
        { OutError = TEXT("HydrologyComponentConflict: macro channel has no downstream node"); return false; }
        const auto& Parent = Nodes[Node.Parent];
        if (Parent.RoutePoint.WaterZ > Node.RoutePoint.WaterZ || Parent.Cell == Node.Cell)
        { OutError = TEXT("HydrologyComponentConflict: invalid macro downstream profile"); return false; }
        if (!bCertifiedGeometryRouteStarted)
        {
            CertifiedGeometryRiver = Plan.Rivers.AddDefaulted();
            bCertifiedGeometryRouteStarted = true;
        }
        auto& River = Plan.Rivers[CertifiedGeometryRiver];
        if (!VoxelGenerationAllocation::RetainArray(River.Points, 2, OutError)) return false;
        River.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
            FIntVector(Node.Cell.X, Node.Cell.Y, 0), 0x4d4143524f524956ull, 0);
        const int64 OwnerSpan = int64(Recipe->Settings.HydrologyCellSize) * Recipe->Settings.HydrologyRegionSide;
        River.OwnerRegion.Coordinate = FIntPoint(int32(VoxelGeneration::FloorDivideWide(Node.Cell.X, OwnerSpan)),
            int32(VoxelGeneration::FloorDivideWide(Node.Cell.Y, OwnerSpan)));
        const auto Midpoint = [](const FIntPoint& First, const FIntPoint& Second)
        {
            return FIntPoint(int32((int64(First.X) + Second.X) / 2), int32((int64(First.Y) + Second.Y) / 2));
        };
        FVoxelRiverRoutePoint Start = Node.RoutePoint;
        FVoxelRiverRoutePoint End = Parent.RoutePoint;
        // A selected source may also receive an existing river. Only a true headwater
        // starts at its cell; internal selected sources must retain the shared joint.
        if (!Node.bHeadwater) Start.Position = Midpoint(Node.Cell, Parent.Cell);
        if (Parent.bHasParent && !Parent.bReceiver) End.Position = Midpoint(Parent.Cell, Parent.ParentCell);
        River.Points.Add(Start);
        River.Points.Add(End);
        bCertifiedGeometryRouteStarted = false;
        ++CertifiedGeometryNode;
    }
    if (CertifiedGeometryNode < Nodes.Num())
    { OutError = TEXT("Yielded: macro river edges"); return false; }
    // 每段只从宏观端点生成确定性曲线；不读取细地表，也不重求水位或河网。
    if (MacroGeometryIndex < Plan.Rivers.Num())
    {
        while (MacroGeometryNode < Nodes.Num() &&
            (Nodes[MacroGeometryNode].bReceiver || Nodes[MacroGeometryNode].bGhost))
        {
            ++MacroGeometryNode;
        }
        if (!Nodes.IsValidIndex(MacroGeometryNode) ||
            !Nodes.IsValidIndex(Nodes[MacroGeometryNode].Parent))
        {
            OutError = TEXT("HydrologyComponentConflict: missing macro curve parent");
            return false;
        }
        const auto& Parent = Nodes[Nodes[MacroGeometryNode].Parent];
        if (!Plan.BuildRiverGeometry(MacroGeometryIndex, OutError, InCancel, &Parent.Cell)) return false;
        ++MacroGeometryIndex;
        ++MacroGeometryNode;
        if (MacroGeometryIndex < Plan.Rivers.Num())
        { OutError = TEXT("Yielded: macro river curves"); return false; }
    }
    UE_LOG(LogTemp, Display, TEXT("Voxel macro river graph ready: selectedNodes=%d ownedCoarseCells=%d rivers=%d rain=%llu received=%llu"),
        Nodes.Num(), Result.DrainageCells.Num(), Plan.Rivers.Num(), Result.TotalRainWeight, Result.ReceivedRainWeight);
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyComponentBuild::FinalizeProfileResult(FString& OutError,
    const TAtomic<bool>* InCancel)
{
    auto& Plan = Result.Plan;
    int32 Work = 512;
    auto Add = [&](uint64 Value) { CertifiedHash = VoxelGeneration::Mix(CertifiedHash ^ VoxelGeneration::Mix(Value)); };
    auto AddPosition = [&](const FIntPoint& P) { Add((uint64(uint32(P.X))<<32)|uint32(P.Y)); };
    while (Work > 0)
    {
        --Work;
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
        if (CertifiedFinalStage == 0)
        {
            if (CertifiedIndexRiver < Plan.Rivers.Num())
            {
                if (CertifiedIndexPoint < Plan.Rivers[CertifiedIndexRiver].MeanderPoints.Num())
                {
                    if (!Plan.ValidateRiverRoutePoint(CertifiedIndexRiver,CertifiedIndexPoint,OutError)) return false;
                    ++CertifiedIndexPoint; continue;
                }
                ++CertifiedIndexRiver; CertifiedIndexPoint = 0; continue;
            }
            CertifiedIndexRiver = 0; CertifiedFallStart = 0; CertifiedFallEnd = 1; CertifiedFinalStage = 1;
        }
        else if (CertifiedFinalStage == 1)
        {
            if (CertifiedIndexRiver < Plan.Rivers.Num())
            {
                const auto& Points = Plan.Rivers[CertifiedIndexRiver].MeanderPoints;
                if (CertifiedFallStart+1 >= Points.Num())
                { ++CertifiedIndexRiver; CertifiedFallStart = 0; CertifiedFallEnd = 1; continue; }
                if (CertifiedFallEnd >= Points.Num())
                { ++CertifiedFallStart; CertifiedFallEnd = CertifiedFallStart+1; continue; }
                const auto& Up = Points[CertifiedFallStart]; const auto& Down = Points[CertifiedFallEnd];
                const double Run = FVector2D(double(int64(Down.Position.X)-Up.Position.X),double(int64(Down.Position.Y)-Up.Position.Y)).Size();
                if (Run>24.0) { ++CertifiedFallStart; CertifiedFallEnd = CertifiedFallStart+1; continue; }
                const int64 Drop = int64(Up.WaterZ)-Down.WaterZ;
                if (Drop<8 || double(Drop)*2.0<Run) { ++CertifiedFallEnd; continue; }
                FVoxelWaterfallSemantic Fall;
                Fall.Upstream = FIntVector(Up.Position.X,Up.Position.Y,Up.WaterZ);
                Fall.Downstream = FIntVector(Down.Position.X,Down.Position.Y,Down.WaterZ);
                Fall.HalfWidthCells = FMath::Min(Up.WaterHalfWidth,Down.WaterHalfWidth);
                Fall.Id.High = VoxelGeneration::Mix((uint64(uint32(Up.Position.X))<<32)|uint32(Up.Position.Y));
                Fall.Id.Low = VoxelGeneration::Mix((uint64(uint32(Down.Position.X))<<32)|uint32(Down.Position.Y)) ^ VoxelGeneration::Mix(uint64(uint32(Plan.RiverSeed)));
                if (Plan.Waterfalls.Num() == Plan.Waterfalls.Max())
                {
                    if (Plan.Waterfalls.Num()==MAX_int32) { OutError=TEXT("Oversized: waterfall semantics"); return false; }
                    const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(Plan.Waterfalls.Max())*2)));
                    if (!VoxelGenerationAllocation::RetainArray(Plan.Waterfalls,Capacity,OutError)) return false;
                }
                Plan.Waterfalls.Add(Fall);
                CertifiedFallStart = CertifiedFallEnd+1; CertifiedFallEnd = CertifiedFallStart+1; continue;
            }
            CertifiedIndexRiver = 0; CertifiedIndexPoint = 1; CertifiedFinalStage = 2;
        }
        else if (CertifiedFinalStage == 2)
        {
            if (CertifiedIndexRiver < Plan.Rivers.Num())
            {
                const auto& Points = Plan.Rivers[CertifiedIndexRiver].MeanderPoints;
                if (CertifiedIndexPoint >= Points.Num()) { ++CertifiedIndexRiver; CertifiedIndexPoint=1; continue; }
                if (!CertifiedIndexBucketActive)
                {
                    const auto& A = Points[CertifiedIndexPoint-1]; const auto& B = Points[CertifiedIndexPoint];
                    const int64 Radius = FMath::Max(int64(A.WaterHalfWidth)+A.BankWidth+A.ShoreWidth,int64(B.WaterHalfWidth)+B.BankWidth+B.ShoreWidth);
                    const int64 MinX = FMath::Max(int64(FMath::Min(A.Position.X,B.Position.X))-Radius,VoxelGeneration::WorldCellMin);
                    const int64 MinY = FMath::Max(int64(FMath::Min(A.Position.Y,B.Position.Y))-Radius,VoxelGeneration::WorldCellMin);
                    const int64 MaxX = FMath::Min(int64(FMath::Max(A.Position.X,B.Position.X))+Radius,VoxelGeneration::WorldCellMaxExclusive-1);
                    const int64 MaxY = FMath::Min(int64(FMath::Max(A.Position.Y,B.Position.Y))+Radius,VoxelGeneration::WorldCellMaxExclusive-1);
                    CertifiedIndexMin = FIntPoint(int32(VoxelGeneration::FloorDivideWide(MinX,Plan.Grid.CellSize)),int32(VoxelGeneration::FloorDivideWide(MinY,Plan.Grid.CellSize)));
                    CertifiedIndexMax = FIntPoint(int32(VoxelGeneration::FloorDivideWide(MaxX,Plan.Grid.CellSize)),int32(VoxelGeneration::FloorDivideWide(MaxY,Plan.Grid.CellSize)));
                    CertifiedIndexCell = CertifiedIndexMin; CertifiedIndexBucketActive = true;
                }
                auto* Bucket = Plan.RiverSegmentsByHydrologyCell.Find(CertifiedIndexCell);
                if (!Bucket)
                {
                    if (Plan.RiverSegmentsByHydrologyCell.Num()==CertifiedIndexCapacity)
                    {
                        if (CertifiedIndexCapacity==MAX_int32) { OutError=TEXT("Oversized: river spatial buckets"); return false; }
                        const int32 Capacity=int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(CertifiedIndexCapacity)*2)));
                        if (!VoxelGenerationAllocation::RetainMap(Plan.RiverSegmentsByHydrologyCell,Capacity,OutError)) return false;
                        CertifiedIndexCapacity=Capacity;
                    }
                    Bucket=&Plan.RiverSegmentsByHydrologyCell.Add(CertifiedIndexCell);
                }
                if (Bucket->Num()==Bucket->Max())
                {
                    if (Bucket->Num()==MAX_int32) { OutError=TEXT("Oversized: river spatial references"); return false; }
                    const int32 Capacity=int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(Bucket->Max())*2)));
                    if (!VoxelGenerationAllocation::RetainArray(*Bucket,Capacity,OutError)) return false;
                }
                Bucket->Add({CertifiedIndexRiver,CertifiedIndexPoint});
                if (CertifiedIndexCell.X == CertifiedIndexMax.X)
                {
                    CertifiedIndexCell.X=CertifiedIndexMin.X;
                    if (CertifiedIndexCell.Y == CertifiedIndexMax.Y) { CertifiedIndexBucketActive=false; ++CertifiedIndexPoint; }
                    else ++CertifiedIndexCell.Y;
                }
                else ++CertifiedIndexCell.X;
                continue;
            }
            CertifiedFinalStage=3;
        }
        else if (CertifiedFinalStage==3)
        {
            if (!Plan.Lakes.IsEmpty())
            { OutError=TEXT("Invalid local lake in macro river publication"); return false; }
            CertifiedHash=VoxelGeneration::Mix(Recipe->RecipeHash ^ 0x5749445448524956ull);
            AddPosition(Result.Key); Add(Result.TotalRainWeight); Add(Result.IncomingRainWeight); Add(Result.ReceivedRainWeight); Add(Result.ExportedRainWeight); Add(uint64(Result.DrainageCells.Num()));
            CertifiedHashCursor=0; CertifiedFinalStage=4;
        }
        else if (CertifiedFinalStage==4)
        {
            if (CertifiedHashCursor<Result.DrainageCells.Num()) { AddPosition(Result.DrainageCells[CertifiedHashCursor++]); continue; }
            Add(uint64(Plan.Rivers.Num())); CertifiedIndexRiver=0; CertifiedIndexPoint=INDEX_NONE; CertifiedFinalStage=5;
        }
        else if (CertifiedFinalStage==5)
        {
            if (CertifiedIndexRiver<Plan.Rivers.Num())
            {
                const auto& River=Plan.Rivers[CertifiedIndexRiver];
                if (CertifiedIndexPoint==INDEX_NONE)
                { Add(River.Id.High); Add(River.Id.Low); Add(uint64(River.MeanderPoints.Num())); CertifiedIndexPoint=0; continue; }
                if (CertifiedIndexPoint<River.MeanderPoints.Num())
                {
                    const auto& P=River.MeanderPoints[CertifiedIndexPoint++];
                    AddPosition(P.Position); Add(uint32(P.WaterZ)); Add(uint32(P.BedHalfWidth)); Add(uint32(P.WaterHalfWidth));
                    Add(uint32(P.BankWidth)); Add(uint32(P.ShoreWidth)); Add(uint32(P.Depth)); Add(P.Accumulation);
                    Add(uint32(P.Curvature)); Add(uint32(P.MeanderOffset)); Add(P.bAnchor?1:0); Add(P.bWalkableMargins?1:0); continue;
                }
                ++CertifiedIndexRiver; CertifiedIndexPoint=INDEX_NONE; continue;
            }
            Result.Fingerprint=CertifiedHash==0?1:CertifiedHash; CertifiedFinalStage=7;
        }
        else { bProfilesSolved = true; OutError.Reset(); return true; }
    }
    OutError=TEXT("Yielded: certified river index and fingerprint"); return false;
}
