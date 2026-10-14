#include "Voxel/Generation/Hydrology/VoxelHydrologyRiverDecoration.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
    const FIntPoint DecorationFaces[4] = { FIntPoint(-1,0), FIntPoint(1,0), FIntPoint(0,-1), FIntPoint(0,1) };
    int32 DecorationRadius(const FVoxelRiverMeanderPoint& P)
    { return P.WaterHalfWidth + P.BankWidth + P.ShoreWidth; }
    int32 DecorationDistance(int64 X, int64 Y)
    {
        X = FMath::Abs(X); Y = FMath::Abs(Y);
        return int32(FMath::Min<int64>(MAX_int32,FMath::Max(X,Y)+FMath::Min(X,Y)*3/8));
    }
    int32 DecorationTangentQ10(int32 Degrees)
    {


        static constexpr int32 Values[] = {0,90,181,274,373,477,591,717,859,
            1024,1221,1462,1774,2196,2813,3822,5807};
        Degrees=FMath::Clamp(Degrees,1,80);
        const int32 Bucket=Degrees/5;
        return Bucket>=16 ? Values[16] : Values[Bucket]+
            (Values[Bucket+1]-Values[Bucket])*(Degrees%5)/5;
    }
    EVoxelHydrologySharedSearchStep DecorationInterrupted(const FString& Error, bool bPending)
    {
        if (Error == TEXT("Canceled")) return EVoxelHydrologySharedSearchStep::Canceled;
        if (Error.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        if (Error.StartsWith(TEXT("Yielded"))) return EVoxelHydrologySharedSearchStep::Yielded;
        return bPending ? EVoxelHydrologySharedSearchStep::NeedInput : EVoxelHydrologySharedSearchStep::Failed;
    }
}

bool VoxelHydrology::EvaluateCertifiedRiverSection(int32 Ground, int32 Water, int32 Distance,
    const FVoxelRiverShape& Shape, FVoxelRiverSection& Out, FString& Error)
{
    if (!Shape.bWalkableMargins) return EvaluateRiverSection(Ground,Water,Distance,Shape,Out,Error);

    if (!EvaluateRiverSection(Ground,Water,Distance,Shape,Out,Error)) return false;
    if (Shape.WaterHalfWidth-Shape.BedHalfWidth < Shape.Depth-1)
    { Error = TEXT("Walkable river margin is too narrow for its depth"); return false; }
    const int32 WetEnd = Shape.WaterHalfWidth;
    const int32 BankEnd = WetEnd+Shape.BankWidth;
    const int32 ShoreEnd = BankEnd+Shape.ShoreWidth;
    FVoxelRiverSection Result;
    Result.WaterPlane = Water; Result.BedPlane = Water-Shape.Depth;
    if (Distance <= Shape.BedHalfWidth)
    {
        Result.GroundPlane = Result.BedPlane;
        Result.Zone = EVoxelRiverSurfaceZone::ChannelBed; Result.bWet = true;
    }
    else if (Distance <= WetEnd)
    {
        Result.GroundPlane = Result.BedPlane + int32(int64(Shape.Depth-1)*
            (Distance-Shape.BedHalfWidth)/FMath::Max(1,WetEnd-Shape.BedHalfWidth));
        Result.Zone = EVoxelRiverSurfaceZone::WetMargin; Result.bWet = true;
    }
    else if (Distance <= ShoreEnd)
    {
        Result.GroundPlane = FMath::Min(Ground,Water-1+Distance-WetEnd);
        Result.Zone = Distance <= BankEnd ? EVoxelRiverSurfaceZone::DryBank : EVoxelRiverSurfaceZone::Floodplain;
    }
    else Result.GroundPlane = Ground;
    Result.bExceededCutFill = FMath::Abs(int64(Result.GroundPlane)-Ground)>Shape.MaxCutFill;
    Out = Result; Error.Reset(); return true;
}

bool FVoxelHydrologyRiverDecoration::Initialize(const TArray<FVoxelRiverRoutePoint>& InRaw,
    const FVoxelHydrologyRiverDecorationSettings& InSettings, FString& Error)
{
    if (Stage != EStage::None || InRaw.Num()<2 ||
        InSettings.DesiredWaterHalfWidth<1 || InSettings.DesiredWaterHalfWidth>256 ||
        InSettings.DesiredDepth<1 || InSettings.DesiredDepth>256 ||
        InSettings.MeanderStrength<0 || InSettings.MeanderStrength>4096 ||
        InSettings.MeanderFrequency<1 || InSettings.MeanderOctaves<1 || InSettings.MeanderOctaves>8 ||
        InSettings.MaxMeanderAngle<1 || InSettings.MaxMeanderAngle>89 ||
        InSettings.JunctionHalfLength<1 || InSettings.JunctionHalfLength>65536 || InSettings.SafeTubeRadius<1 ||
        InSettings.SafeTubeRadius>65536 || InSettings.MaxCutFill<1 || InSettings.MaxCutFill>512 ||
        InSettings.MinimumGroundZ < -32768 || InSettings.MinimumGroundZ>32765 ||
        InSettings.WorldMinimum.X>InSettings.WorldMaximum.X || InSettings.WorldMinimum.Y>InSettings.WorldMaximum.Y ||
        InSettings.WalkableWindowLength<16 || InSettings.WalkableWindowLength>1024 ||
        InSettings.WalkableClearanceHalfWidth<1 || InSettings.WalkableClearanceHalfWidth>2 ||
        InSettings.WalkableLandingLength<1 || InSettings.WalkableLandingLength>32)
    { Error = TEXT("Invalid river decoration initialization"); return false; }
    const auto& Initial = InRaw[0];
    if (Initial.BedHalfWidth<1 || Initial.BedHalfWidth>Initial.WaterHalfWidth || Initial.WaterHalfWidth>256 ||
        Initial.BankWidth<1 || Initial.BankWidth>512 || Initial.ShoreWidth<1 || Initial.ShoreWidth>1024 ||
        Initial.Depth<1 || Initial.Depth>256)
    { Error=TEXT("Invalid certified base shape"); return false; }
    Raw = &InRaw; Settings = InSettings;
    const auto& First = InRaw[0]; const auto& Last = InRaw.Last();
    const int64 DX = int64(Last.Position.X)-First.Position.X, DY = int64(Last.Position.Y)-First.Position.Y;
    if ((DX != 0 && DY != 0) || FMath::Abs(DX)+FMath::Abs(DY) != InRaw.Num()-1)
    { Raw = nullptr; Error = TEXT("Decoration requires a straight certified unit port"); return false; }
    Direction = FIntPoint(DX>0?1:DX<0?-1:0,DY>0?1:DY<0?-1:0);
    DesiredWater = FMath::Max(First.WaterHalfWidth,Settings.DesiredWaterHalfWidth);
    DesiredBed = FMath::Max(First.BedHalfWidth,DesiredWater-1);
    DesiredDepth = FMath::Max(First.Depth,Settings.DesiredDepth);
    WalkWater = FMath::Max(First.WaterHalfWidth,First.BedHalfWidth+First.Depth-1);
    WalkRadius = WalkWater+First.BankWidth+First.ShoreWidth;
    MaximumRadius = FMath::Max(DesiredWater,WalkWater)+First.BankWidth+First.ShoreWidth;
    Protection = Settings.JunctionHalfLength+WalkRadius+2;
    ControlStep = FMath::Clamp(Settings.MeanderFrequency/12,8,16);
    Cursor = 0; LastAnchor = 0; RunStart = Protection;
    Stage = EStage::Validate; Error.Reset(); return true;
}

FVoxelRiverMeanderPoint FVoxelHydrologyRiverDecoration::BasePoint(int32 Index) const
{
    const auto& P = (*Raw)[Index]; FVoxelRiverMeanderPoint R;
    R.Position = P.Position; R.WaterZ = P.WaterZ; R.BedHalfWidth = P.BedHalfWidth;
    R.WaterHalfWidth = P.WaterHalfWidth; R.BankWidth = P.BankWidth; R.ShoreWidth = P.ShoreWidth;
    R.Depth = P.Depth; R.Accumulation = P.Accumulation; R.bAnchor = true;
    R.bWalkableMargins = false; return R;
}

FVoxelRiverMeanderPoint FVoxelHydrologyRiverDecoration::DecoratedPoint(int32 Index) const
{
    auto P = BasePoint(Index);
    const int32 Length = Raw->Num()-1;


    const int32 EndRoom = FMath::Min(Index,Length-Index)-Settings.JunctionHalfLength-ControlStep-1;
    const int32 Room = FMath::Min(Settings.SafeTubeRadius,EndRoom);
    const int32 BaseRadius = DecorationRadius(P);
    const bool bWalk = WalkBegin != INDEX_NONE && Index>=WalkBegin-1 && Index<=WalkEnd+1;
    const bool bWalkShoulder = WalkBegin != INDEX_NONE && Index>=WalkBegin-ControlStep && Index<=WalkEnd+ControlStep;
    if (bWalk)
    {
        P.WaterHalfWidth = WalkWater; P.bWalkableMargins = true;
    }
    else if (Room>BaseRadius && !bWalkShoulder)
    {
        const int32 Growth = FMath::Min(DesiredWater-P.WaterHalfWidth,Room-BaseRadius);
        P.WaterHalfWidth += Growth;
        P.BedHalfWidth = FMath::Min(DesiredBed,FMath::Max(P.BedHalfWidth,P.WaterHalfWidth-1));


        const int32 Taper = FMath::Min(8,Room-BaseRadius);
        P.Depth += (DesiredDepth-P.Depth)*Taper/8;
    }
    const int32 OffsetRoom = FMath::Min(Settings.MeanderStrength,FMath::Min(
        Settings.SafeTubeRadius-DecorationRadius(P),EndRoom-DecorationRadius(P)));
    if (!bWalkShoulder && OffsetRoom>0 && AnchorDistance[Index]>=ControlStep)
    {
        int64 Noise = 0, Weight = 0;
        for (int32 Octave=0; Octave<Settings.MeanderOctaves; ++Octave)
        {
            const int32 OctaveWeight = 1 << (Settings.MeanderOctaves-Octave-1);
            Noise += int64(VoxelGeneration::Noise2D(Settings.Seed,P.Position.X,P.Position.Y,
                FMath::Max(8,Settings.MeanderFrequency>>Octave),0x52495645524D4541ull+Octave))*OctaveWeight;
            Weight += OctaveWeight;
        }
        const int32 Offset = int32(Noise*OffsetRoom/FMath::Max<int64>(1,Weight*32768));
        P.Position.X = int32(int64(P.Position.X)-int64(Direction.Y)*Offset);
        P.Position.Y = int32(int64(P.Position.Y)+int64(Direction.X)*Offset);
        P.MeanderOffset = Offset;
    }
    P.bAnchor = Index==0 || Index==Length || AnchorDistance[Index]==0 || bWalkShoulder;
    return P;
}

bool FVoxelHydrologyRiverDecoration::InWorld(const FIntPoint& P) const
{ return P.X>=Settings.WorldMinimum.X && P.Y>=Settings.WorldMinimum.Y && P.X<=Settings.WorldMaximum.X && P.Y<=Settings.WorldMaximum.Y; }

bool FVoxelHydrologyRiverDecoration::IsBaseWet(const FIntPoint& Cell, int32 WaterZ) const
{
    const auto& Start = (*Raw)[0].Position;
    const int64 Along = (int64(Cell.X)-Start.X)*Direction.X+(int64(Cell.Y)-Start.Y)*Direction.Y;
    const int32 Index = int32(FMath::Clamp<int64>(Along,0,Raw->Num()-1));
    const auto& P = (*Raw)[Index];
    return P.WaterZ==WaterZ && DecorationDistance(int64(Cell.X)-P.Position.X,
        int64(Cell.Y)-P.Position.Y)<=P.WaterHalfWidth;
}

bool FVoxelHydrologyRiverDecoration::IsBaseContact(const FIntPoint& Cell, int32 WaterZ) const
{
    if (IsBaseWet(Cell,WaterZ)) return true;
    for (const auto& D : DecorationFaces)
    {
        const int64 X = int64(Cell.X)+D.X, Y = int64(Cell.Y)+D.Y;
        if (X>=Settings.WorldMinimum.X && Y>=Settings.WorldMinimum.Y && X<=Settings.WorldMaximum.X && Y<=Settings.WorldMaximum.Y &&
            IsBaseWet(FIntPoint(int32(X),int32(Y)),WaterZ)) return true;
    }
    return false;
}

bool FVoxelHydrologyRiverDecoration::CheckSample(const FVoxelHydrologySectionSample& S, FString& Error) const
{
    if (S.OriginalGround < -32768 || S.OriginalGround>32768 ||
        (S.ExistingWaterZ!=MIN_int32 && (S.ExistingWaterZ < -32768 || S.ExistingWaterZ>32768 ||
        S.OriginalGround>=S.ExistingWaterZ || (S.ExistingBodyId.High==0 && S.ExistingBodyId.Low==0))))
    { Error = TEXT("Invalid static sample in river decoration"); return false; }
    return true;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyRiverDecoration::ReadAt(FReadSample Read,
    const FIntPoint& Cell, FVoxelHydrologySectionSample& Out, FString& Error) const
{
    const auto Status = Read(Cell,Out,Error);
    if (Status != EVoxelHydrologyClosureRead::Ready)
        return DecorationInterrupted(Error,Status==EVoxelHydrologyClosureRead::Pending);
    return CheckSample(Out,Error) ? EVoxelHydrologySharedSearchStep::Ready : EVoxelHydrologySharedSearchStep::Failed;
}

void FVoxelHydrologyRiverDecoration::RejectCandidate()
{
    Points.Reset(); Cursor = 0; Stage = EStage::Fallback; bBaseFallback = true;
}

void FVoxelHydrologyRiverDecoration::NextRasterCell()
{
    RasterFace = -1; RasterSegment = 1; RasterBestDistance = MAX_int32;
    if (RasterCell.X==RasterMax.X)
    {
        RasterCell.X=RasterMin.X;
        if (RasterCell.Y==RasterMax.Y) bRasterStarted=false;
        else ++RasterCell.Y;
    }
    else ++RasterCell.X;
}

EVoxelHydrologySharedSearchStep FVoxelHydrologyRiverDecoration::Advance(FReadSample Read,
    int32 Budget, FString& Error, const TAtomic<bool>* Cancel)
{
    Error.Reset();
    if (Cancel && Cancel->Load()) { Error=TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
    if (Stage==EStage::None) { Error=TEXT("River decoration is not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (Stage==EStage::Ready) return EVoxelHydrologySharedSearchStep::Ready;
    if (Budget<=0) return EVoxelHydrologySharedSearchStep::Yielded;
    const int32 Count = Raw->Num(), Length = Count-1;
    if (Points.Max()<Count && !VoxelGenerationAllocation::RetainArray(Points,Count,Error)) return DecorationInterrupted(Error,false);
    if (AnchorDistance.Max()<Count && !VoxelGenerationAllocation::RetainArray(AnchorDistance,Count,Error)) return DecorationInterrupted(Error,false);
    while (Budget>0)
    {
        --Budget;
        if (Cancel && Cancel->Load()) { Error=TEXT("Canceled"); return EVoxelHydrologySharedSearchStep::Canceled; }
        if (Stage==EStage::Validate)
        {
            const auto& P = (*Raw)[Cursor]; const auto& Base = (*Raw)[0];
            const int64 X = int64(Base.Position.X)+int64(Direction.X)*Cursor;
            const int64 Y = int64(Base.Position.Y)+int64(Direction.Y)*Cursor;
            if (!InWorld(P.Position) || P.Position.X!=X || P.Position.Y!=Y ||
                P.WaterZ < Settings.MinimumGroundZ+P.Depth || P.WaterZ>32766 ||
                P.BedHalfWidth<1 || P.BedHalfWidth>P.WaterHalfWidth || P.WaterHalfWidth>256 ||
                P.BankWidth<1 || P.BankWidth>512 || P.ShoreWidth<1 || P.ShoreWidth>1024 || P.Depth<1 || P.Depth>256 ||
                P.BedHalfWidth!=Base.BedHalfWidth || P.WaterHalfWidth!=Base.WaterHalfWidth ||
                P.BankWidth!=Base.BankWidth || P.ShoreWidth!=Base.ShoreWidth || P.Depth!=Base.Depth ||
                (Cursor>0 && P.WaterZ>(*Raw)[Cursor-1].WaterZ))
            { Error=TEXT("Invalid immutable certified raw route for decoration"); return EVoxelHydrologySharedSearchStep::Failed; }
            const bool bTransition = Cursor==0 || Cursor==Length ||
                (Cursor>0 && P.WaterZ!=(*Raw)[Cursor-1].WaterZ) ||
                (Cursor<Length && P.WaterZ!=(*Raw)[Cursor+1].WaterZ);
            if (bTransition) LastAnchor=Cursor;
            AnchorDistance.Add(Cursor-LastAnchor);
            if (Cursor>=Protection && Cursor<=Length-Protection)
            {
                if (Cursor==Protection || P.WaterZ!=(*Raw)[Cursor-1].WaterZ) RunStart=Cursor;
                if (LongestStart==INDEX_NONE || Cursor-RunStart>LongestEnd-LongestStart)
                { LongestStart=RunStart; LongestEnd=Cursor; }
            }
            if (++Cursor<Count) continue;

            const bool bLongPort = Length>2*(Protection+ControlStep)+Settings.WalkableWindowLength;
            const bool bSelectLanding = bLongPort || VoxelGeneration::Noise2D(Settings.Seed,
                (*Raw)[0].Position.X,(*Raw)[0].Position.Y,Settings.MeanderFrequency,
                0x57414C4B57494E44ull)>=0;
            if (Settings.bEnableWalkableMargins && bSelectLanding && WalkWater<=256 && WalkRadius<=Settings.SafeTubeRadius &&
                LongestStart!=INDEX_NONE && LongestEnd-LongestStart>=Settings.WalkableWindowLength)
            {
                WalkBegin=LongestStart+(LongestEnd-LongestStart-Settings.WalkableWindowLength)/2;
                WalkEnd=WalkBegin+Settings.WalkableWindowLength;
            }
            Cursor=Length; LastAnchor=Length; Stage=EStage::Backward;
        }
        else if (Stage==EStage::Backward)
        {
            if (AnchorDistance[Cursor]==0) LastAnchor=Cursor;
            AnchorDistance[Cursor]=FMath::Min(AnchorDistance[Cursor],LastAnchor-Cursor);
            if (--Cursor>=0) continue;
            Cursor=0; Stage=EStage::Build;
        }
        else if (Stage==EStage::Build)
        {
            const bool bWalkBoundary = WalkBegin!=INDEX_NONE &&
                (Cursor==WalkBegin-2 || Cursor==WalkBegin-1 || Cursor==WalkBegin || Cursor==WalkEnd || Cursor==WalkEnd+1 || Cursor==WalkEnd+2);
            if (Cursor==0 || Cursor==Length || AnchorDistance[Cursor]==0 || Cursor%ControlStep==0 || bWalkBoundary)
                Points.Add(DecoratedPoint(Cursor));
            if (++Cursor<Count) continue;
            Cursor=1; Stage=EStage::Angles;
        }
        else if (Stage==EStage::Angles)
        {
            if (Cursor+1<Points.Num())
            {
                const auto& A=Points[Cursor-1]; const auto& B=Points[Cursor]; const auto& C=Points[Cursor+1];
                const int64 AX=int64(B.Position.X)-A.Position.X, AY=int64(B.Position.Y)-A.Position.Y;
                const int64 BX=int64(C.Position.X)-B.Position.X, BY=int64(C.Position.Y)-B.Position.Y;
                const int64 Dot=AX*BX+AY*BY, Cross=AX*BY-AY*BX;
                if (Dot<=0 || double(FMath::Abs(Cross))*1024>
                    double(Dot)*DecorationTangentQ10(Settings.MaxMeanderAngle))
                { RejectCandidate(); continue; }
                ++Cursor; continue;
            }
            const int64 Radius=int64(FMath::Min(MaximumRadius,Settings.SafeTubeRadius))+Settings.MeanderStrength+1;
            const auto& A=(*Raw)[0].Position; const auto& B=Raw->Last().Position;
            const int64 MinX=int64(FMath::Min(A.X,B.X))-Radius, MinY=int64(FMath::Min(A.Y,B.Y))-Radius;
            const int64 MaxX=int64(FMath::Max(A.X,B.X))+Radius, MaxY=int64(FMath::Max(A.Y,B.Y))+Radius;


            if (MinX<Settings.WorldMinimum.X || MinY<Settings.WorldMinimum.Y ||
                MaxX>Settings.WorldMaximum.X || MaxY>Settings.WorldMaximum.Y)
            { RejectCandidate(); continue; }
            RasterMin=FIntPoint(int32(MinX),int32(MinY)); RasterMax=FIntPoint(int32(MaxX),int32(MaxY));
            RasterCell=RasterMin; bRasterStarted=true; RasterSegment=1; RasterFace=-1; RasterBestDistance=MAX_int32; Stage=EStage::Raster;
        }
        else if (Stage==EStage::Raster)
        {
            if (!bRasterStarted)
            {
                if (WalkBegin==INDEX_NONE) { Stage=EStage::Ready; return EVoxelHydrologySharedSearchStep::Ready; }

                WalkAlong=WalkBegin+Settings.WalkableClearanceHalfWidth+3;
                WalkSide=-1; WalkLateral=0; WalkLane=-Settings.WalkableClearanceHalfWidth;
                WalkScanSegment=1; WalkBestDistance=MAX_int32;
                Stage=EStage::Walk; continue;
            }
            if (RasterSegment<Points.Num())
            {
                FVoxelRiverProjection P;
                if (!VoxelHydrology::ProjectRiverSegment(Points[RasterSegment-1],Points[RasterSegment],RasterCell,P))
                { Error=TEXT("Invalid candidate river projection"); return EVoxelHydrologySharedSearchStep::Failed; }
                if (P.DistanceCells<=P.Shape.WaterHalfWidth+P.Shape.BankWidth+P.Shape.ShoreWidth && P.DistanceCells<RasterBestDistance)
                { RasterProjection=P; RasterBestDistance=P.DistanceCells; }
                ++RasterSegment; continue;
            }
            if (RasterBestDistance==MAX_int32) { NextRasterCell(); continue; }
            if (RasterFace==-1)
            {
                const auto Status=ReadAt(Read,RasterCell,RasterSample,Error);
                if (Status!=EVoxelHydrologySharedSearchStep::Ready) return Status;
                if (!VoxelHydrology::EvaluateCertifiedRiverSection(RasterSample.OriginalGround,
                    RasterProjection.WaterPlane,RasterProjection.DistanceCells,RasterProjection.Shape,RasterSection,Error))
                { RejectCandidate(); Error.Reset(); continue; }
                RasterSection.GroundPlane=FMath::Min(RasterSample.OriginalGround,RasterSection.GroundPlane);
                const int32 WetBank=RasterProjection.Shape.WaterHalfWidth+RasterProjection.Shape.BankWidth;
                if (RasterSample.ExistingWaterZ!=MIN_int32)
                {
                    if (RasterSection.bWet && (RasterSample.ExistingWaterZ!=RasterProjection.WaterPlane ||
                        !IsBaseContact(RasterCell,RasterSample.ExistingWaterZ))) { RejectCandidate(); continue; }
                }
                else if (RasterSection.GroundPlane<Settings.MinimumGroundZ ||
                    int64(RasterSection.GroundPlane)<int64(RasterSample.OriginalGround)-Settings.MaxCutFill ||
                    (RasterProjection.DistanceCells<=WetBank && RasterProjection.WaterPlane>RasterSample.OriginalGround))
                { RejectCandidate(); continue; }


                const auto& Start=(*Raw)[0].Position;
                const int64 Along=(int64(RasterCell.X)-Start.X)*Direction.X+(int64(RasterCell.Y)-Start.Y)*Direction.Y;
                const int64 Across=-(int64(RasterCell.X)-Start.X)*Direction.Y+(int64(RasterCell.Y)-Start.Y)*Direction.X;
                if (FMath::Abs(Across)>Settings.SafeTubeRadius) { RejectCandidate(); continue; }
                if (Along<=Settings.JunctionHalfLength || Along>=Length-Settings.JunctionHalfLength)
                {
                    const int32 Index=int32(FMath::Clamp<int64>(Along,0,Length)); const auto P=BasePoint(Index);
                    FVoxelRiverShape Shape; Shape.BedHalfWidth=P.BedHalfWidth; Shape.WaterHalfWidth=P.WaterHalfWidth;
                    Shape.BankWidth=P.BankWidth; Shape.ShoreWidth=P.ShoreWidth; Shape.Depth=P.Depth; Shape.MaxCutFill=Settings.MaxCutFill;
                    FVoxelRiverSection Base;
                    if (!VoxelHydrology::EvaluateRiverSection(RasterSample.OriginalGround,P.WaterZ,
                        DecorationDistance(int64(RasterCell.X)-P.Position.X,int64(RasterCell.Y)-P.Position.Y),Shape,Base,Error))
                        return EVoxelHydrologySharedSearchStep::Failed;
                    Base.GroundPlane=FMath::Min(RasterSample.OriginalGround,Base.GroundPlane);
                    if (Base.GroundPlane!=RasterSection.GroundPlane || Base.bWet!=RasterSection.bWet ||
                        (Base.bWet && Base.WaterPlane!=RasterSection.WaterPlane)) { RejectCandidate(); continue; }
                }
                RasterFace=0; continue;
            }
            if (RasterFace<4)
            {
                const auto& D=DecorationFaces[RasterFace];
                const int64 X=int64(RasterCell.X)+D.X, Y=int64(RasterCell.Y)+D.Y;
                if (X<Settings.WorldMinimum.X || Y<Settings.WorldMinimum.Y || X>Settings.WorldMaximum.X || Y>Settings.WorldMaximum.Y)
                { RejectCandidate(); continue; }
                const FIntPoint Neighbor{int32(X),int32(Y)}; FVoxelHydrologySectionSample Adjacent;
                const auto Status=ReadAt(Read,Neighbor,Adjacent,Error);
                if (Status!=EVoxelHydrologySharedSearchStep::Ready) return Status;
                if (Adjacent.ExistingWaterZ!=MIN_int32 && RasterSample.ExistingWaterZ==MIN_int32)
                {
                    if (RasterSection.bWet)
                    {
                        if (RasterProjection.WaterPlane!=Adjacent.ExistingWaterZ || !IsBaseContact(Neighbor,Adjacent.ExistingWaterZ))
                        { RejectCandidate(); continue; }
                    }
                    else if (RasterSection.GroundPlane<Adjacent.ExistingWaterZ) { RejectCandidate(); continue; }
                }
                ++RasterFace; continue;
            }
            NextRasterCell();
        }
        else if (Stage==EStage::Walk)
        {
            const auto& Origin=(*Raw)[WalkAlong].Position;
            const int64 X=int64(Origin.X)+int64(Direction.X)*WalkLane-int64(Direction.Y)*WalkSide*WalkLateral;
            const int64 Y=int64(Origin.Y)+int64(Direction.Y)*WalkLane+int64(Direction.X)*WalkSide*WalkLateral;
            if (X<Settings.WorldMinimum.X || Y<Settings.WorldMinimum.Y || X>Settings.WorldMaximum.X || Y>Settings.WorldMaximum.Y)
            { RejectCandidate(); continue; }
            const FIntPoint Cell{int32(X),int32(Y)};
            if (WalkScanSegment<Points.Num())
            {
                FVoxelRiverProjection P;
                if (!VoxelHydrology::ProjectRiverSegment(Points[WalkScanSegment-1],Points[WalkScanSegment],Cell,P))
                { Error=TEXT("Invalid walkable corridor projection"); return EVoxelHydrologySharedSearchStep::Failed; }
                if (P.DistanceCells<=P.Shape.WaterHalfWidth+P.Shape.BankWidth+P.Shape.ShoreWidth && P.DistanceCells<WalkBestDistance)
                { WalkProjection=P; WalkBestDistance=P.DistanceCells; }
                ++WalkScanSegment; continue;
            }
            FVoxelHydrologySectionSample Sample; const auto Status=ReadAt(Read,Cell,Sample,Error);
            if (Status!=EVoxelHydrologySharedSearchStep::Ready) return Status;

            if (Sample.ExistingWaterZ!=MIN_int32) { RejectCandidate(); continue; }
            int32 Ground=Sample.OriginalGround;
            if (WalkBestDistance!=MAX_int32)
            {
                FVoxelRiverSection S;
                if (!VoxelHydrology::EvaluateCertifiedRiverSection(Ground,WalkProjection.WaterPlane,
                    WalkProjection.DistanceCells,WalkProjection.Shape,S,Error)) { RejectCandidate(); Error.Reset(); continue; }
                Ground=FMath::Min(Ground,S.GroundPlane);
            }
            const int32 Slot=WalkLane+Settings.WalkableClearanceHalfWidth;
            if (WalkLateral>0 && FMath::Abs(int64(Ground)-WalkPrevious[Slot])>1) { RejectCandidate(); continue; }
            if (Slot>0 && FMath::Abs(int64(Ground)-WalkPrevious[Slot-1])>1) { RejectCandidate(); continue; }


            if (WalkLateral>0 && Slot>0 && FMath::Abs(int64(Ground)-WalkLastOldHeight)>1)
            { RejectCandidate(); continue; }
            if (WalkLateral>0 && Slot+1<=2*Settings.WalkableClearanceHalfWidth &&
                FMath::Abs(int64(Ground)-WalkPrevious[Slot+1])>1) { RejectCandidate(); continue; }
            if (WalkLateral>=WalkRadius && Ground<(*Raw)[WalkAlong].WaterZ) { RejectCandidate(); continue; }
            WalkLastOldHeight=WalkPrevious[Slot];
            WalkPrevious[Slot]=Ground;
            WalkBestDistance=MAX_int32; WalkScanSegment=1;
            if (++WalkLane<=Settings.WalkableClearanceHalfWidth) continue;
            WalkLane=-Settings.WalkableClearanceHalfWidth;
            if (++WalkLateral<=WalkRadius+Settings.WalkableLandingLength) continue;
            WalkLateral=0;
            if (WalkSide==-1) { WalkSide=1; continue; }
            WalkSide=-1;
            if (++WalkAlong<=WalkEnd-Settings.WalkableClearanceHalfWidth-3) continue;
            Stage=EStage::Ready; return EVoxelHydrologySharedSearchStep::Ready;
        }
        else if (Stage==EStage::Fallback)
        {
            Points.Add(BasePoint(Cursor));
            if (++Cursor<Count) continue;
            Stage=EStage::Ready; return EVoxelHydrologySharedSearchStep::Ready;
        }
        else if (Stage==EStage::Ready) return EVoxelHydrologySharedSearchStep::Ready;
    }
    return EVoxelHydrologySharedSearchStep::Yielded;
}

TArray<FVoxelRiverMeanderPoint> FVoxelHydrologyRiverDecoration::TakePoints()
{ check(IsReady()); return MoveTemp(Points); }

uint64 FVoxelHydrologyRiverDecoration::GetAllocatedBytes() const
{ return Points.GetAllocatedSize()+AnchorDistance.GetAllocatedSize(); }

void FVoxelHydrologyRiverDecoration::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Points);
    VoxelGenerationAllocation::FreeRetainedArray(AnchorDistance);
    *this=FVoxelHydrologyRiverDecoration();
}
