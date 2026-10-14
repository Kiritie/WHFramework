#include "Voxel/Generation/Hydrology/VoxelHydrologyComponent.h"

#include "Voxel/Generation/Hydrology/VoxelHydrologyBasinClosure.h"
#include "Voxel/Generation/Hydrology/VoxelHydrologyProfile.h"
#include "Voxel/Generation/Hydrology/VoxelRiverCorridor.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"

bool FVoxelHydrologyComponentBuild::BuildGeometry(FTileLookup InLookup, FString& OutError,
    const TAtomic<bool>* InCancel)
{
    auto& Plan = Result.Plan;
    if (!bCertifiedGeometryInitialized)
    {
        if (!VoxelGenerationAllocation::RetainArray(Plan.Rivers,Nodes.Num(),OutError)) return false;
        Plan.RiverSeed = Recipe->Settings.Seed;
        Plan.RiverShapeSmoothingPasses = 0; Plan.RiverMeanderStrength = Recipe->Settings.RiverMeanderStrength;
        Plan.RiverMeanderFrequency = Recipe->Settings.RiverMeanderFrequency;
        Plan.RiverMeanderOctaves = Recipe->Settings.RiverMeanderOctaves;
        Plan.RiverMaxMeanderAngle = Recipe->Settings.RiverMaxMeanderAngle;
        Plan.Grid.CellSize = Recipe->Settings.HydrologyCellSize;
        Plan.bCompleteRiverIndex = true;
        Plan.CoreMin = FIntPoint(int32(VoxelGeneration::WorldCellMin),int32(VoxelGeneration::WorldCellMin));
        Plan.CoreMax = FIntPoint(int32(VoxelGeneration::WorldCellMaxExclusive),int32(VoxelGeneration::WorldCellMaxExclusive));
        bCertifiedGeometryInitialized = true;
    }
    int32 Work = 256;
    while (CertifiedGeometryNode < Nodes.Num() && Work > 0)
    {
        --Work;
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
        const auto& Node = Nodes[CertifiedGeometryNode];
        FIntPoint End;
        const TArray<int64>* Heights = nullptr;
        if (Node.bReceiver)
        {
            const auto* Certificate = WidthRouting.FindNode(Node.Cell);
            if (!Certificate || Certificate->Interval.FixedWaterZ == MIN_int32)
            { OutError = TEXT("HydrologyComponentConflict: missing root connector certificate"); return false; }
            End = Certificate->ConnectorEnd;
            if (End == Node.Cell) { ++CertifiedGeometryNode; continue; }

        }
        else
        {
            if (!Nodes.IsValidIndex(Node.Parent)) { OutError = TEXT("Missing certified geometry parent"); return false; }
            End = Nodes[Node.Parent].Cell;
            const auto Step = WidthRouting.AdvanceRestoreSelectedPort(Node.Cell,Node.RoutePoint.WaterZ,
                Nodes[Node.Parent].RoutePoint.WaterZ,256,OutError,InCancel);
            if (Step != EVoxelHydrologySharedSearchStep::Ready)
            {
                if (Step == EVoxelHydrologySharedSearchStep::Yielded) OutError = TEXT("Yielded: exact width path restoration");
                else if (Step == EVoxelHydrologySharedSearchStep::Canceled) OutError = TEXT("Canceled");
                else if (Step == EVoxelHydrologySharedSearchStep::CapacityBlocked && OutError.IsEmpty()) OutError = TEXT("CapacityBlocked: exact width path restoration");
                return false;
            }
            Heights = &WidthRouting.GetRestoredHeights();
        }
        if (End.X != Node.Cell.X && End.Y != Node.Cell.Y)
        { OutError = TEXT("Invalid nonaxis certified geometry"); return false; }
        const int64 Length = FMath::Abs(int64(End.X)-Node.Cell.X)+FMath::Abs(int64(End.Y)-Node.Cell.Y);
        if (Length < 1 || Length >= MAX_int32 || (Heights && Heights->Num() != Length+1))
        { OutError = TEXT("Invalid restored fine path length"); return false; }
        const int32 Count = int32(Length+1);
        if (!bCertifiedGeometryRouteStarted)
        {
            CertifiedGeometryRiver = Plan.Rivers.AddDefaulted();
            auto& River = Plan.Rivers[CertifiedGeometryRiver];
            River.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
                FIntVector(Node.Cell.X,Node.Cell.Y,Node.bReceiver ? 1 : 0),0x5749445448524956ull,0);
            const int64 OwnerSpan = int64(Recipe->Settings.HydrologyCellSize)*Recipe->Settings.HydrologyRegionSide;
            River.OwnerRegion.Coordinate = FIntPoint(int32(VoxelGeneration::FloorDivideWide(Node.Cell.X,OwnerSpan)),
                int32(VoxelGeneration::FloorDivideWide(Node.Cell.Y,OwnerSpan)));
            River.Min = FIntPoint(FMath::Min(Node.Cell.X,End.X),FMath::Min(Node.Cell.Y,End.Y));
            River.Max = FIntPoint(FMath::Max(Node.Cell.X,End.X),FMath::Max(Node.Cell.Y,End.Y));
            CertifiedGeometryPoint = 0; bCertifiedGeometryRouteStarted = true;
            bDecorationReady=false; DecorationBoundsCursor=0;
        }
        auto& River = Plan.Rivers[CertifiedGeometryRiver];
        if (!VoxelGenerationAllocation::RetainArray(River.Points,Count,OutError) ||
            !VoxelGenerationAllocation::RetainArray(River.ShapePoints,Count,OutError) ||
            !VoxelGenerationAllocation::RetainArray(River.MeanderPoints,Count,OutError)) return false;
        const FIntPoint Direction(End.X>Node.Cell.X ? 1 : End.X<Node.Cell.X ? -1 : 0,
            End.Y>Node.Cell.Y ? 1 : End.Y<Node.Cell.Y ? -1 : 0);
        while (CertifiedGeometryPoint < Count && Work > 0)
        {
            --Work;
            auto Point = Node.RoutePoint;
            Point.Position = FIntPoint(int32(int64(Node.Cell.X)+int64(Direction.X)*CertifiedGeometryPoint),
                int32(int64(Node.Cell.Y)+int64(Direction.Y)*CertifiedGeometryPoint));
            if (Heights)
            {
                const int64 H = (*Heights)[CertifiedGeometryPoint];
                if (H < -32768 || H > 32766) { OutError = TEXT("Invalid restored water range"); return false; }
                Point.WaterZ = int32(H);
            }
            FVoxelRiverShapePoint ShapePoint;
            ShapePoint.Position = Point.Position; ShapePoint.bAnchor = true; ShapePoint.WaterZ = Point.WaterZ;
            ShapePoint.BedHalfWidth = Point.BedHalfWidth; ShapePoint.WaterHalfWidth = Point.WaterHalfWidth;
            ShapePoint.BankWidth = Point.BankWidth; ShapePoint.ShoreWidth = Point.ShoreWidth;
            ShapePoint.Depth = Point.Depth; ShapePoint.Accumulation = Point.Accumulation;
            FVoxelRiverMeanderPoint Meander;
            Meander.Position = Point.Position; Meander.bAnchor = true; Meander.WaterZ = Point.WaterZ;
            Meander.BedHalfWidth = Point.BedHalfWidth; Meander.WaterHalfWidth = Point.WaterHalfWidth;
            Meander.BankWidth = Point.BankWidth; Meander.ShoreWidth = Point.ShoreWidth;
            Meander.Depth = Point.Depth; Meander.Accumulation = Point.Accumulation;
            River.Points.Add(Point); River.ShapePoints.Add(ShapePoint); River.MeanderPoints.Add(Meander);
            ++CertifiedGeometryPoint;
        }
        if (CertifiedGeometryPoint < Count) { OutError = TEXT("Yielded: certified fine geometry transfer"); return false; }
        if (!Node.bReceiver && !bDecorationReady)
        {
            if (!bDecorationStarted)
            {
                FVoxelHydrologyRiverDecorationSettings Settings;
                Settings.DesiredWaterHalfWidth=Node.DesiredWaterHalfWidth; Settings.DesiredDepth=Node.DesiredDepth;
                Settings.Seed=Recipe->Settings.Seed; Settings.MeanderStrength=Recipe->Settings.RiverMeanderStrength;
                Settings.MeanderFrequency=Recipe->Settings.RiverMeanderFrequency;
                Settings.MeanderOctaves=Recipe->Settings.RiverMeanderOctaves;
                Settings.MaxMeanderAngle=Recipe->Settings.RiverMaxMeanderAngle;
                Settings.JunctionHalfLength=RoutingShape.WaterHalfWidth+RoutingShape.BankWidth+RoutingShape.ShoreWidth+1;
                Settings.SafeTubeRadius=(RoutingGrid.GetSpacing()-1)/2;
                Settings.MaxCutFill=RoutingShape.MaxCutFill; Settings.MinimumGroundZ=FMath::Max(-32768,Recipe->Settings.MinZ);
                Settings.WorldMinimum=FIntPoint(int32(VoxelGeneration::WorldCellMin),int32(VoxelGeneration::WorldCellMin));
                Settings.WorldMaximum=FIntPoint(int32(VoxelGeneration::WorldCellMaxExclusive-1),int32(VoxelGeneration::WorldCellMaxExclusive-1));
                if (!Decoration.Initialize(River.Points,Settings,OutError)) return false;
                bDecorationStarted=true;
            }
            auto Reader=[&](const FIntPoint& Cell,FVoxelHydrologySectionSample& Sample,FString& Error)
            {
                FVoxelHydrologyReceivingVoxel Fine;
                const auto Status=ReadFineWater(Cell,InLookup,Fine,Error);
                if (Status!=EVoxelHydrologyClosureRead::Ready) return Status;
                Sample.OriginalGround=Fine.OriginalGroundZ; Sample.ExistingWaterZ=Fine.WaterZ; Sample.ExistingBodyId=Fine.BodyId;
                return EVoxelHydrologyClosureRead::Ready;
            };
            const auto Step=Decoration.Advance(Reader,256,OutError,InCancel);
            if (Step!=EVoxelHydrologySharedSearchStep::Ready)
            {
                if (Step==EVoxelHydrologySharedSearchStep::Yielded) OutError=TEXT("Yielded: certified river appearance");
                else if (Step==EVoxelHydrologySharedSearchStep::Canceled) OutError=TEXT("Canceled");
                else if (Step==EVoxelHydrologySharedSearchStep::CapacityBlocked && OutError.IsEmpty()) OutError=TEXT("CapacityBlocked: river appearance");
                return false;
            }
            VoxelGenerationAllocation::FreeRetainedArray(River.MeanderPoints);
            River.MeanderPoints=Decoration.TakePoints(); Decoration.FreeBacking(); bDecorationStarted=false; bDecorationReady=true;
        }
        while (DecorationBoundsCursor<River.MeanderPoints.Num() && Work>0)
        {
                --Work; const auto& P=River.MeanderPoints[DecorationBoundsCursor++];
                River.Min.X=FMath::Min(River.Min.X,P.Position.X); River.Min.Y=FMath::Min(River.Min.Y,P.Position.Y);
                River.Max.X=FMath::Max(River.Max.X,P.Position.X); River.Max.Y=FMath::Max(River.Max.Y,P.Position.Y);
        }
        if (DecorationBoundsCursor<River.MeanderPoints.Num()) { OutError=TEXT("Yielded: certified geometry bounds"); return false; }
        bCertifiedGeometryRouteStarted = false; ++CertifiedGeometryNode;
    }
    if (CertifiedGeometryNode < Nodes.Num()) { OutError = TEXT("Yielded: certified geometry build"); return false; }
    if (CertifiedRiverSortStage == 0)
    {
        CertifiedRiverSortIndex = Plan.Rivers.Num()/2-1; CertifiedRiverSortEnd = Plan.Rivers.Num()-1;
        CertifiedRiverSortStage = 1;
    }
    auto Less = [&](int32 A,int32 B)
    {
        const auto& X = Plan.Rivers[A].Points[0].Position;
        const auto& Y = Plan.Rivers[B].Points[0].Position;
        return X.X != Y.X ? X.X<Y.X : X.Y != Y.Y ? X.Y<Y.Y : Plan.Rivers[A].Id<Plan.Rivers[B].Id;
    };
    auto Sift = [&](int32 Root,int32 Last)
    {
        while (int64(Root)*2+1 <= Last)
        {
            int32 Child = Root*2+1;
            if (Child<Last && Less(Child,Child+1)) ++Child;
            if (!Less(Root,Child)) break;
            Swap(Plan.Rivers[Root],Plan.Rivers[Child]); Root = Child;
        }
    };
    while (Work > 0 && CertifiedRiverSortStage < 3)
    {
        --Work;
        if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
        if (CertifiedRiverSortStage == 1)
        {
            if (CertifiedRiverSortIndex >= 0) Sift(CertifiedRiverSortIndex--,CertifiedRiverSortEnd);
            else CertifiedRiverSortStage = 2;
        }
        else if (CertifiedRiverSortEnd > 0)
        { Swap(Plan.Rivers[0],Plan.Rivers[CertifiedRiverSortEnd]); Sift(0,--CertifiedRiverSortEnd); }
        else CertifiedRiverSortStage = 3;
    }
    if (CertifiedRiverSortStage < 3) { OutError = TEXT("Yielded: canonical river output order"); return false; }
    OutError.Reset(); return true;
}

bool FVoxelHydrologyComponentBuild::FinalizeCertifiedFootprint(FTileLookup InLookup,
    FString& OutError, const TAtomic<bool>* InCancel)
{
    int32 Work = 512;
    while (CertifiedRasterRiver < Result.Plan.Rivers.Num() && Work > 0)
    {
        const auto& River = Result.Plan.Rivers[CertifiedRasterRiver];
        if (!bCertifiedRasterStarted)
        {
            while (CertifiedRasterInitPoint<River.MeanderPoints.Num() && Work>0)
            {
                --Work; const auto& P=River.MeanderPoints[CertifiedRasterInitPoint++];
                CertifiedRasterRadius=FMath::Max(CertifiedRasterRadius,P.WaterHalfWidth+P.BankWidth+P.ShoreWidth);
            }
            if (CertifiedRasterInitPoint<River.MeanderPoints.Num()) break;
            CertifiedRasterX=River.Min.X-CertifiedRasterRadius; CertifiedRasterY=River.Min.Y-CertifiedRasterRadius;
            CertifiedRasterSegment=1; CertifiedRasterBestDistance=MAX_int32; bCertifiedRasterStarted=true;
        }
        const int32 Radius=CertifiedRasterRadius;
        const int32 MaxX=River.Max.X+Radius, MaxY=River.Max.Y+Radius;
        while (CertifiedRasterY <= MaxY && Work > 0)
        {
            if (InCancel && InCancel->Load()) { OutError=TEXT("Canceled"); return false; }
            const FIntPoint Position(CertifiedRasterX,CertifiedRasterY);
            while (CertifiedRasterSegment<River.MeanderPoints.Num() && Work>0)
            {
                --Work;
                FVoxelRiverProjection Projection;
                if (VoxelHydrology::ProjectRiverSegment(River.MeanderPoints[CertifiedRasterSegment-1],
                    River.MeanderPoints[CertifiedRasterSegment],Position,Projection) &&
                    Projection.DistanceCells<=Projection.Shape.WaterHalfWidth+Projection.Shape.BankWidth+Projection.Shape.ShoreWidth &&
                    Projection.DistanceCells<CertifiedRasterBestDistance)
                { CertifiedRasterProjection=Projection; CertifiedRasterBestDistance=Projection.DistanceCells; }
                ++CertifiedRasterSegment;
            }
            if (CertifiedRasterSegment<River.MeanderPoints.Num() || Work<=0) break;
            --Work;
            const int32 Distance=CertifiedRasterBestDistance;
            const auto& Shape=CertifiedRasterProjection.Shape;
            const int32 WaterZ=CertifiedRasterProjection.WaterPlane;
            if (Distance != MAX_int32)
            {
                const FIntPoint Cell(CertifiedRasterX,CertifiedRasterY);
                FVoxelHydrologyReceivingVoxel Fine;
                if (ReadFineWater(Cell,InLookup,Fine,OutError) != EVoxelHydrologyClosureRead::Ready) return false;
                if (Fine.WaterZ != MIN_int32)
                {
                    if (Distance <= Shape.WaterHalfWidth && Fine.WaterZ != WaterZ)
                    { OutError = TEXT("HydrologyComponentConflict: final wet footprint changed receiver plane"); return false; }
                    if (!Result.ReceivingWaterByVoxel.Contains(Cell))
                    {
                        if (Result.ReceivingWaterByVoxel.Num() == ReceivingVoxelCapacity)
                        {
                            if (ReceivingVoxelCapacity == MAX_int32) { OutError = TEXT("Oversized: receiving footprint"); return false; }
                            const int32 Capacity = int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(ReceivingVoxelCapacity)*2)));
                            if (!VoxelGenerationAllocation::RetainMap(Result.ReceivingWaterByVoxel,Capacity,OutError)) return false;
                            ReceivingVoxelCapacity = Capacity;
                        }
                        if (CertifiedReceivingOrder.Num()==CertifiedReceivingOrder.Max())
                        {
                            if (CertifiedReceivingOrder.Num()==MAX_int32) { OutError=TEXT("Oversized: receiving footprint order"); return false; }
                            const int32 Capacity=int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(CertifiedReceivingOrder.Max())*2)));
                            if (!VoxelGenerationAllocation::RetainArray(CertifiedReceivingOrder,Capacity,OutError)) return false;
                        }
                        Result.ReceivingWaterByVoxel.Add(Cell,Fine);
                        CertifiedReceivingOrder.Add(Cell);
                    }
                }
                else
                {
                    FVoxelRiverSection Section;
                    if (!VoxelHydrology::EvaluateCertifiedRiverSection(Fine.OriginalGroundZ,WaterZ,Distance,Shape,Section,OutError)) return false;
                    const int32 Ground = FMath::Min(Fine.OriginalGroundZ,Section.GroundPlane);
                    if (int64(Ground)<int64(Fine.OriginalGroundZ)-RoutingShape.MaxCutFill ||
                        (Distance<=Shape.WaterHalfWidth+Shape.BankWidth && WaterZ>Fine.OriginalGroundZ))
                    { OutError = TEXT("HydrologyComponentConflict: final geometry violates certified section"); return false; }
                }
            }
            CertifiedRasterSegment=1; CertifiedRasterBestDistance=MAX_int32;
            if (CertifiedRasterX == MaxX) { CertifiedRasterX = River.Min.X-Radius; ++CertifiedRasterY; }
            else ++CertifiedRasterX;
        }
        if (CertifiedRasterY <= MaxY) { OutError = TEXT("Yielded: final certified footprint"); return false; }
        ++CertifiedRasterRiver; bCertifiedRasterStarted = false; CertifiedRasterInitPoint=CertifiedRasterRadius=0;
    }
    if (CertifiedRasterRiver < Result.Plan.Rivers.Num()) { OutError = TEXT("Yielded: final certified footprint"); return false; }
    bProfilesSolved = true;
    return FinalizeProfileResult(OutError,InCancel);
}

bool FVoxelHydrologyComponentBuild::ResolveProfiles(FTileLookup InLookup,
    FString& OutError, const TAtomic<bool>* InCancel)
{
    if (bCertifiedGeometryInitialized) return FinalizeCertifiedFootprint(InLookup,OutError,InCancel);
    if (bProfilesSolved)
    {
        return FinalizeProfileResult(OutError, InCancel);
    }
    FVoxelGenerationWorkspace Workspace;
    bool bLookupFailed = false;
    FString LookupError;

    auto ReadReceivingPlane = [&](const FIntPoint& Cell, int32& Plane, FVoxelStableId& BodyId) -> bool
    {
        FVoxelHydrologyReceivingVoxel Water;
        FString Error;
        const auto Read = ReadFineWater(Cell, InLookup, Water, Error);
        if (Read != EVoxelHydrologyClosureRead::Ready)
        {
            bLookupFailed = true;
            LookupError = MoveTemp(Error);
            return false;
        }
        Plane = Water.WaterZ;
        BodyId = Water.BodyId;
        return true;
    };
    auto ReturnLookupFailure = [&]()
    {

        OutError = LookupError;
        return false;
    };

    auto RetryCanonicalGeometry = [&]()
    {
        if (bProfileRawFallback) return false;
        ProfileClosure.FreeBacking();
        bProfileClosureActive = false;
        VoxelGenerationAllocation::FreeRetainedMap(Result.LakeWaterByVoxel);
        VoxelGenerationAllocation::FreeRetainedMap(Result.ReceivingWaterByVoxel);
        Result.ExpandedReceivingWater = FVoxelHydrologyReceivingVoxel();
        ReceivingVoxelCapacity = 0;
        bHasAddedLakeReceiver = false;
        AddedLakeReceiverId = FVoxelStableId();
        bProfileRawFallback = true;
        bGeometryBuilt = false;
        OutError = TEXT("Yielded: receiving constraints require canonical river geometry");
        return true;
    };

    int64 StationCount64 = 0;
    int64 SegmentCount64 = 0;
    for (const auto& River : Result.Plan.Rivers)
    {
        StationCount64 += River.MeanderPoints.Num();
        SegmentCount64 += FMath::Max(0, River.MeanderPoints.Num() - 1);
    }
    if (StationCount64 >= MAX_int32 || SegmentCount64 >= MAX_int32)
    { OutError = TEXT("Oversized: component profile stations"); return false; }
    const int32 StationCount = int32(StationCount64);
    const int32 SegmentCount = int32(SegmentCount64);
    TArray<FVoxelRiverProfileNode> ProfileNodes;
    TArray<FVoxelRiverProfileEdge> Edges;
    TArray<int32> PointIds, RouteOffsets, TerrainCaps, OriginalCaps, ReceivingPlanes, EdgeCaps;
    TArray<FIntPoint> Positions, LowestCells;
    TArray<FVoxelStableId> ReceivingIds;
    TArray<int32> NextDownstream;
    TMap<FIntPoint, int32> EndpointByPosition;
    if (!Workspace.ReserveArray(ProfileNodes, StationCount, OutError) ||
        !Workspace.ReserveArray(Edges, SegmentCount, OutError) ||
        !Workspace.ReserveArray(PointIds, StationCount, OutError) ||
        !Workspace.ReserveArray(RouteOffsets, Result.Plan.Rivers.Num() + 1, OutError) ||
        !Workspace.ReserveArray(TerrainCaps, StationCount, OutError) ||
        !Workspace.ReserveArray(OriginalCaps, StationCount, OutError) ||
        !Workspace.ReserveArray(ReceivingPlanes, StationCount, OutError) ||
        !Workspace.ReserveArray(EdgeCaps, SegmentCount, OutError) ||
        !Workspace.ReserveArray(Positions, StationCount, OutError) ||
        !Workspace.ReserveArray(ReceivingIds, StationCount, OutError) ||
        !Workspace.ReserveArray(NextDownstream, StationCount, OutError) ||
        !Workspace.ReserveArray(LowestCells, SegmentCount, OutError) ||
        !Workspace.ReserveMap(EndpointByPosition, StationCount, OutError)) return false;
    for (const auto& River : Result.Plan.Rivers)
    {
        RouteOffsets.Add(PointIds.Num());
        for (int32 PointIndex = 0; PointIndex < River.MeanderPoints.Num(); ++PointIndex)
        {
            const auto& Point = River.MeanderPoints[PointIndex];
            const bool bEndpoint = PointIndex == 0 || PointIndex + 1 == River.MeanderPoints.Num();
            const int32* Existing = bEndpoint ? EndpointByPosition.Find(Point.Position) : nullptr;
            int32 Id = Existing ? *Existing : INDEX_NONE;
            if (Id == INDEX_NONE)
            {
                Id = ProfileNodes.AddDefaulted();
                ProfileNodes[Id].LowerWaterZ = FMath::Max(-32768, Recipe->Settings.MinZ) + Point.Depth;
                ProfileNodes[Id].UpperWaterZ = 32766;
                OriginalCaps.Add(Point.WaterZ);
                TerrainCaps.Add(32766);
                ReceivingPlanes.Add(MIN_int32);
                ReceivingIds.Add(FVoxelStableId());
                NextDownstream.Add(INDEX_NONE);
                Positions.Add(Point.Position);
                if (bEndpoint) EndpointByPosition.Add(Point.Position, Id);
            }
            else
            {
                OriginalCaps[Id] = FMath::Min(OriginalCaps[Id], Point.WaterZ);
                ProfileNodes[Id].LowerWaterZ = FMath::Max(ProfileNodes[Id].LowerWaterZ, FMath::Max(-32768, Recipe->Settings.MinZ) + Point.Depth);
            }
            PointIds.Add(Id);
        }
    }
    RouteOffsets.Add(PointIds.Num());


    for (const auto& Node : Nodes)
    {
        if (!Node.bReceiver) continue;
        if (const int32* Id = EndpointByPosition.Find(Node.RoutePoint.Position))
        { ReceivingPlanes[*Id] = Node.ReceiverWaterZ; ReceivingIds[*Id] = Node.ReceiverId; }
    }
    for (int32 RiverIndex = 0; RiverIndex < Result.Plan.Rivers.Num(); ++RiverIndex)
    {
        const auto& River = Result.Plan.Rivers[RiverIndex];
        for (int32 PointIndex = 1; PointIndex < River.MeanderPoints.Num(); ++PointIndex)
        {
            if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
            const int32 Up = PointIds[RouteOffsets[RiverIndex] + PointIndex - 1];
            const int32 Down = PointIds[RouteOffsets[RiverIndex] + PointIndex];
            int32 TouchedPlane = MIN_int32;
            FVoxelStableId TouchedId;
            bool bMismatchedBodies = false;
            auto IsReceivingWater = [&](const FIntPoint& Cell)
            {
                int32 Plane = MIN_int32;
                FVoxelStableId BodyId;
                if (!ReadReceivingPlane(Cell, Plane, BodyId) || Plane == MIN_int32) return false;
                if (Terrain->SampleSurfaceHeight(Cell.X, Cell.Y) >= Plane) return false;
                if (!Result.ReceivingWaterByVoxel.Find(Cell))
                {
                    if (Result.ReceivingWaterByVoxel.Num() == ReceivingVoxelCapacity)
                    {
                        const int64 Capacity = FMath::Max<int64>(16, int64(ReceivingVoxelCapacity) * 2);
                        FString Error;
                        if (Capacity > MAX_int32 || !VoxelGenerationAllocation::RetainMap(
                            Result.ReceivingWaterByVoxel, int32(Capacity), Error))
                        {
                            bLookupFailed = true;
                            LookupError = Capacity > MAX_int32 ? TEXT("Oversized: receiving-water voxel map") : Error;
                            return false;
                        }
                        ReceivingVoxelCapacity = int32(Capacity);
                    }
                    const auto OceanId = VoxelGeneration::MakeStableId(Recipe->Settings.Seed,
                        FIntVector(0, 0, Recipe->Settings.SeaLevel), 0x4F4345414E524543ull, 0);
                    Result.ReceivingWaterByVoxel.Add(Cell,
                        FVoxelHydrologyReceivingVoxel{Terrain->SampleSurfaceHeight(Cell.X, Cell.Y), Plane, BodyId, BodyId == OceanId});
                }
                if (TouchedPlane != MIN_int32 && TouchedPlane != Plane) bMismatchedBodies = true;
                if (TouchedPlane == MIN_int32 || BodyId < TouchedId) TouchedId = BodyId;
                TouchedPlane = Plane;
                return true;
            };
            int32 Cap = 32766;
            FIntPoint Lowest = River.MeanderPoints[PointIndex].Position;
            const bool bCapReady = VoxelHydrology::SampleRiverCorridorCap(
                River.MeanderPoints[PointIndex - 1], River.MeanderPoints[PointIndex], *Terrain,
                IsReceivingWater, Cap, Lowest, OutError, InCancel);
            if (bLookupFailed) return ReturnLookupFailure();
            if (!bCapReady) return false;
            if (bMismatchedBodies)
            { OutError = TEXT("RiverProfileConflict: one final segment intersects fixed water bodies with incompatible identities or levels"); return false; }
            if (TouchedPlane != MIN_int32)
            {
                for (const int32 Id : {Up, Down})
                {
                    if (ReceivingPlanes[Id] != MIN_int32 && ReceivingPlanes[Id] != TouchedPlane)
                    { OutError = TEXT("RiverProfileConflict: incompatible receiving-water junction"); return false; }
                    if (ReceivingPlanes[Id] == MIN_int32 || TouchedId < ReceivingIds[Id]) ReceivingIds[Id] = TouchedId;
                    ReceivingPlanes[Id] = TouchedPlane;
                }
            }
            TerrainCaps[Up] = FMath::Min(TerrainCaps[Up], Cap);
            TerrainCaps[Down] = FMath::Min(TerrainCaps[Down], Cap);
            if (NextDownstream[Up] != INDEX_NONE && NextDownstream[Up] != Down)
            { OutError = TEXT("RiverProfileConflict: a canonical station has multiple downstream receivers"); return false; }
            NextDownstream[Up] = Down;
            Edges.Add({Up, Down});
            EdgeCaps.Add(Cap);
            LowestCells.Add(Lowest);
        }
    }
    for (int32 Index = 0; Index < ProfileNodes.Num(); ++Index)
    {
        auto& Node = ProfileNodes[Index];
        Node.bFixed = ReceivingPlanes[Index] != MIN_int32;
        Node.FixedWaterZ = Node.bFixed ? ReceivingPlanes[Index] : 0;


        Node.UpperWaterZ = FMath::Min(TerrainCaps[Index],
            Node.bFixed ? Node.FixedWaterZ : OriginalCaps[Index]);
    }
    TArray<int32> Water;
    int32 Conflict = INDEX_NONE;
    int32 RequiredWater = MIN_int32;


    if (!VoxelHydrology::SolveRiverProfile(ProfileNodes, Edges, Water, Conflict,
        OutError, InCancel, &RequiredWater))
    {
        VoxelGenerationAllocation::FreeRetainedArray(Water);
        if (RequiredWater == MIN_int32 || Conflict == INDEX_NONE) return false;
        auto ReceiverFor = [&](int32 Start) -> int32
        {
            int32 Cursor = Start;
            for (int32 Count = 0; Cursor != INDEX_NONE && Count < ProfileNodes.Num(); ++Count)
            {
                if (ReceivingPlanes[Cursor] == RequiredWater) return Cursor;
                Cursor = NextDownstream[Cursor];
            }
            return INDEX_NONE;
        };
        auto Reaches = [&](int32 From, int32 To)
        {
            for (int32 Count = 0; From != INDEX_NONE && Count < ProfileNodes.Num(); ++Count)
            {
                if (From == To) return true;
                From = NextDownstream[From];
            }
            return false;
        };
        const int32 Target = ReceiverFor(Conflict);
        FIntPoint Seed;
        bool bFoundSeed = false;
        if (Target != INDEX_NONE)
        {
            for (int32 Index = 0; Index < Edges.Num(); ++Index)
            {
                if (EdgeCaps[Index] >= RequiredWater ||
                    (!Reaches(Edges[Index].Upstream, Conflict) && !Reaches(Conflict, Edges[Index].Upstream))) continue;
                const int32 CandidateTarget = ReceiverFor(Edges[Index].Upstream);
                if (CandidateTarget != INDEX_NONE && ReceivingIds[CandidateTarget] == ReceivingIds[Target] &&
                    Terrain->SampleSurfaceHeight(LowestCells[Index].X, LowestCells[Index].Y) < RequiredWater)
                {
                    Seed = LowestCells[Index];
                    bFoundSeed = true;
                    break;
                }
            }
            if (!bFoundSeed && Terrain->SampleSurfaceHeight(Positions[Conflict].X, Positions[Conflict].Y) < RequiredWater)
            { Seed = Positions[Conflict]; bFoundSeed = true; }
        }
        if (bFoundSeed)
        {

            if (RetryCanonicalGeometry()) return false;
            OutError = TEXT("RiverProfileConflict: certified fine basin requires joint corridor rerouting");
            return false;
        }


        if (RetryCanonicalGeometry()) return false;
        OutError = TEXT("RiverProfileConflict: canonical route and fixed receiver need a routing decision");
        return false;
    }
    for (int32 RiverIndex = 0; RiverIndex < Result.Plan.Rivers.Num(); ++RiverIndex)
    {
        auto& River = Result.Plan.Rivers[RiverIndex];
        for (int32 PointIndex = 0; PointIndex < River.MeanderPoints.Num(); ++PointIndex)
            River.MeanderPoints[PointIndex].WaterZ = Water[PointIds[RouteOffsets[RiverIndex] + PointIndex]];
    }
    VoxelGenerationAllocation::FreeRetainedArray(Water);
    bProfilesSolved = true;

    return FinalizeProfileResult(OutError, InCancel);
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
            if (!Result.LakeWaterByVoxel.IsEmpty() || !Plan.Lakes.IsEmpty())
            { OutError=TEXT("Invalid dynamic lake in static certified river publication"); return false; }
            CertifiedHash=VoxelGeneration::Mix(Recipe->RecipeHash ^ 0x5749445448524956ull);
            AddPosition(Result.Key); Add(0x42554b4f55543031ull); Add(Result.RunoffFingerprint); Add(Result.TotalRainWeight); Add(Result.ReceivedRainWeight); Add(uint64(Result.RunoffOutlets.Num())); Add(uint64(Result.DrainageCells.Num()));
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
            Add(uint64(CertifiedReceivingOrder.Num())); CertifiedHashCursor=0; CertifiedFinalStage=6;
        }
        else if (CertifiedFinalStage==6)
        {
            if (CertifiedHashCursor<CertifiedReceivingOrder.Num())
            {
                const auto& Cell=CertifiedReceivingOrder[CertifiedHashCursor++];
                const auto& Water=Result.ReceivingWaterByVoxel.FindChecked(Cell);
                AddPosition(Cell); Add(uint32(Water.WaterZ)); Add(Water.BodyId.High); Add(Water.BodyId.Low); Add(Water.bOcean?1:0); continue;
            }
            Result.Fingerprint=CertifiedHash==0?1:CertifiedHash; CertifiedFinalStage=7;
        }
        else { OutError.Reset(); return true; }
    }
    OutError=TEXT("Yielded: certified river index and fingerprint"); return false;
}
