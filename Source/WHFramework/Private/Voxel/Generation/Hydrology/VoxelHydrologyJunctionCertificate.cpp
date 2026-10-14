#include "Voxel/Generation/Hydrology/VoxelHydrologyJunctionCertificate.h"

bool FVoxelHydrologyJunctionCertificate::Initialize(const FIntPoint& InCenter, const FVoxelRiverShape& InShape,
    int32 InMinimumGround, const FIntPoint& InWorldMin, const FIntPoint& InWorldMax, FString& Error)
{
    if (bInitialized) { Error = TEXT("Junction certificate already initialized"); return false; }
    Center = InCenter; Shape = InShape; MinimumGround = InMinimumGround;
    WorldMin = InWorldMin; WorldMax = InWorldMax;
    if (!Section.Initialize(Center,Shape,MinimumGround,WorldMin,WorldMax,Error)) return false;
    HalfLength = Shape.WaterHalfWidth + Shape.BankWidth + Shape.ShoreWidth + 1;
    Result.LowerWaterZ = MinimumGround + Shape.Depth; Result.UpperWaterZ = 32766;
    bInitialized = bSectionStarted = true; return true;
}
EVoxelHydrologySharedSearchStep FVoxelHydrologyJunctionCertificate::Advance(
    FVoxelHydrologySectionCertificate::FReadSample Read, int32 WorkBudget, FString& Error, const TAtomic<bool>* Cancel)
{
    Error.Reset();
    if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;
    if (!bInitialized) { Error = TEXT("Junction certificate not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    if (WorkBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    static const FIntPoint Directions[] = {FIntPoint(-1,0),FIntPoint(1,0),FIntPoint(0,-1),FIntPoint(0,1)};
    FIntPoint Position = Center;
    if (Index > 0)
    {
        const int32 Face = (Index - 1) / HalfLength;
        const int32 Distance = (Index - 1) % HalfLength + 1;
        const int64 X = int64(Center.X) + int64(Directions[Face].X)*Distance;
        const int64 Y = int64(Center.Y) + int64(Directions[Face].Y)*Distance;
        if (X < WorldMin.X || Y < WorldMin.Y || X > WorldMax.X || Y > WorldMax.Y)
        { bIneligible = true; bReady = true; Result.bEligible = false; return EVoxelHydrologySharedSearchStep::Ready; }
        Position = FIntPoint(int32(X),int32(Y));
    }
    if (!bSectionStarted)
    {
        if (!Section.Initialize(Position,Shape,MinimumGround,WorldMin,WorldMax,Error)) return EVoxelHydrologySharedSearchStep::Failed;
        bSectionStarted = true;
    }
    const auto Step = Section.Advance(Read,WorkBudget,Error,Cancel);
    if (Step != EVoxelHydrologySharedSearchStep::Ready) return Step;
    const auto& Interval = Section.GetResult();
    if (!Interval.bEligible) bIneligible = true;
    Result.LowerWaterZ = FMath::Max(Result.LowerWaterZ,Interval.LowerWaterZ);
    Result.UpperWaterZ = FMath::Min(Result.UpperWaterZ,Interval.UpperWaterZ);
    if (Interval.FixedWaterZ != MIN_int32)
    {
        if (Result.FixedWaterZ != MIN_int32 && Result.FixedWaterZ != Interval.FixedWaterZ) bIneligible = true;
        const bool Better = Result.FixedWaterZ == MIN_int32 || Interval.FixedBodyId < Result.FixedBodyId ||
            (Interval.FixedBodyId == Result.FixedBodyId && (Position.X < ConnectorEnd.X ||
             (Position.X == ConnectorEnd.X && Position.Y < ConnectorEnd.Y)));
        if (Better)
        { Result.FixedBodyId = Interval.FixedBodyId; Result.FixedConnector = Interval.FixedConnector; ConnectorEnd = Position; }
        Result.FixedWaterZ = Interval.FixedWaterZ;
    }
    ++Index; Section.Reset(); bSectionStarted = false;
    if (Index > 4*HalfLength)
    {
        Result.bEligible = !bIneligible && Result.LowerWaterZ <= Result.UpperWaterZ;
        bReady = true; return EVoxelHydrologySharedSearchStep::Ready;
    }
    return EVoxelHydrologySharedSearchStep::Yielded;
}
void FVoxelHydrologyJunctionCertificate::Reset()
{
    Section.Reset(); Result = FVoxelHydrologySectionInterval();
    MinimumGround = HalfLength = Index = 0;
    bInitialized = bSectionStarted = bIneligible = bReady = false;
}
