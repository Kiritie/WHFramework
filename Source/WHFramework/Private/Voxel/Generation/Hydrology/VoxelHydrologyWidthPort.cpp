#include "Voxel/Generation/Hydrology/VoxelHydrologyWidthPort.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

bool FVoxelHydrologyWidthPort::Initialize(const FIntPoint& Start, const FIntPoint& InEnd,
    const FVoxelRiverShape& InShape, int32 InMinimumGround, const FIntPoint& InWorldMin,
    const FIntPoint& InWorldMax, FString& Error)
{
    if (bInitialized || (Start.X != InEnd.X && Start.Y != InEnd.Y) || Start == InEnd ||
        Start.X < InWorldMin.X || Start.Y < InWorldMin.Y || Start.X > InWorldMax.X || Start.Y > InWorldMax.Y ||
        InEnd.X < InWorldMin.X || InEnd.Y < InWorldMin.Y || InEnd.X > InWorldMax.X || InEnd.Y > InWorldMax.Y)
    { Error = TEXT("Invalid physical width port"); return false; }
    Position = Start; End = InEnd; Shape = InShape; MinimumGround = InMinimumGround;
    WorldMin = InWorldMin; WorldMax = InWorldMax;
    Direction = FIntPoint(InEnd.X > Start.X ? 1 : InEnd.X < Start.X ? -1 : 0,
        InEnd.Y > Start.Y ? 1 : InEnd.Y < Start.Y ? -1 : 0);

    uint64 Hash = 1469598103934665603ull;
    for (uint32 Value : { uint32(Start.X), uint32(Start.Y), uint32(InEnd.X), uint32(InEnd.Y) })
    { Hash ^= Value; Hash *= 1099511628211ull; }
    Result.Transfer.PathId = Hash;
    Result.Transfer.FootprintVersion = 1;
    bInitialized = true; Error.Reset(); return true;
}
bool FVoxelHydrologyWidthPort::SetStartEndpointEqualityBlock(int32 HalfLength,
    const FVoxelHydrologySectionInterval& InStartBlock, FString& Error)
{
    const int64 Length = FMath::Abs(int64(End.X)-Position.X) + FMath::Abs(int64(End.Y)-Position.Y);
    if (!bInitialized || bSectionStarted || !Intervals.IsEmpty() ||
        HalfLength < Shape.WaterHalfWidth + Shape.BankWidth + Shape.ShoreWidth + 1 || Length <= HalfLength)
    { Error = TEXT("Invalid start endpoint equality block"); return false; }
    EqualityHalfLength = HalfLength; StartBlock = InStartBlock; bEndEqualityBlock = false;
    Error.Reset(); return true;
}
bool FVoxelHydrologyWidthPort::SetEndpointEqualityBlocks(int32 HalfLength,
    const FVoxelHydrologySectionInterval& InStartBlock, const FVoxelHydrologySectionInterval& InEndBlock, FString& Error)
{
    const int64 Length = FMath::Abs(int64(End.X)-Position.X) + FMath::Abs(int64(End.Y)-Position.Y);
    if (!bInitialized || bSectionStarted || !Intervals.IsEmpty() || HalfLength < Shape.WaterHalfWidth + Shape.BankWidth + Shape.ShoreWidth + 1 || Length <= int64(HalfLength)*2)
    { Error = TEXT("Invalid endpoint equality blocks"); return false; }
    EqualityHalfLength = HalfLength; StartBlock = InStartBlock; EndBlock = InEndBlock; bEndEqualityBlock = true;
    Error.Reset(); return true;
}
EVoxelHydrologySharedSearchStep FVoxelHydrologyWidthPort::Advance(
    FVoxelHydrologySectionCertificate::FReadSample Read, int32 WorkBudget, FString& Error, const TAtomic<bool>* Cancel)
{
    Error.Reset();
    if (Cancel && Cancel->Load()) return EVoxelHydrologySharedSearchStep::Canceled;
    if (!bInitialized) { Error = TEXT("Width port is not initialized"); return EVoxelHydrologySharedSearchStep::Failed; }
    if (bReady) return EVoxelHydrologySharedSearchStep::Ready;
    if (WorkBudget <= 0) return EVoxelHydrologySharedSearchStep::Yielded;
    if (!bPathComplete)
    {
        const bool bStartBlock = EqualityHalfLength > 0 && Intervals.IsEmpty();
        const int64 DistanceToEnd = FMath::Abs(int64(End.X)-Position.X) + FMath::Abs(int64(End.Y)-Position.Y);
        const bool bEndBlock = bEndEqualityBlock && EqualityHalfLength > 0 && !bStartBlock && DistanceToEnd <= EqualityHalfLength;
        int32 Repeat = 1;
        if (bStartBlock || bEndBlock)
        {
            Terminal = bStartBlock ? StartBlock : EndBlock;
            Repeat = bStartBlock && StartBlock.FixedWaterZ != MIN_int32 ? 1 : EqualityHalfLength + 1;
        }
        else
        {
            if (!bSectionStarted)
            {
                if (!Section.Initialize(Position,Shape,MinimumGround,WorldMin,WorldMax,Error)) return EVoxelHydrologySharedSearchStep::Failed;
                bSectionStarted = true;
            }
            const auto Step = Section.Advance(Read,WorkBudget,Error,Cancel);
            if (Step != EVoxelHydrologySharedSearchStep::Ready) return Step;
            Terminal = Section.GetResult();
        }
        if (!Terminal.bEligible)
        { Result.Downstream = Position; Result.Transfer.bFeasible = false; bReady = true; return EVoxelHydrologySharedSearchStep::Ready; }
        if (Intervals.Num() == MAX_int32) { Error = TEXT("Oversized: width port"); return EVoxelHydrologySharedSearchStep::Failed; }
        if (Intervals.Num() == Intervals.Max() && !VoxelGenerationAllocation::RetainArray(Intervals,
            int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(Intervals.Max())*2))),Error))
            return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        if (ExpansionCounts.Num() == ExpansionCounts.Max() && !VoxelGenerationAllocation::RetainArray(ExpansionCounts,
            int32(FMath::Min<int64>(MAX_int32,FMath::Max<int64>(16,int64(ExpansionCounts.Max())*2))),Error))
            return EVoxelHydrologySharedSearchStep::CapacityBlocked;
        Intervals.Add({Terminal.LowerWaterZ,Terminal.UpperWaterZ});
        ExpansionCounts.Add(Repeat);
        if (Terminal.FixedWaterZ != MIN_int32 || Position == End || bEndBlock)
        { Result.Downstream = bEndBlock ? End : Position; bPathComplete = true; }
        else
        { Position.X += Direction.X * Repeat; Position.Y += Direction.Y * Repeat; Section.Reset(); bSectionStarted = false; }

        return EVoxelHydrologySharedSearchStep::Yielded;
    }
    FVoxelHydrologyWidthEdgeTransfer Transfer;
    const auto Step = Compose.Advance(Intervals,WorkBudget,true,Cancel && Cancel->Load(),Transfer);
    if (Step == EVoxelHydrologyWidthStep::Canceled) return EVoxelHydrologySharedSearchStep::Canceled;
    if (Step == EVoxelHydrologyWidthStep::Pending) return EVoxelHydrologySharedSearchStep::Yielded;
    if (Step == EVoxelHydrologyWidthStep::CertifiedInfeasible)
    { Result.Transfer.bFeasible = false; bReady = true; return EVoxelHydrologySharedSearchStep::Ready; }
    if (Step != EVoxelHydrologyWidthStep::Ready)
    { Error = TEXT("Invalid complete width path"); return EVoxelHydrologySharedSearchStep::Failed; }
    Transfer.PathId = Result.Transfer.PathId; Transfer.FootprintVersion = Result.Transfer.FootprintVersion;
    Result.Transfer = Transfer; bReady = true; return EVoxelHydrologySharedSearchStep::Ready;
}
void FVoxelHydrologyWidthPort::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Intervals);
    VoxelGenerationAllocation::FreeRetainedArray(ExpansionCounts);
    EqualityHalfLength = 0; bEndEqualityBlock = false;
    Section.Reset(); Compose.Reset(); Result = FVoxelHydrologyWidthSearchEdge();
    Terminal = FVoxelHydrologySectionInterval();
    bInitialized = bSectionStarted = bPathComplete = bReady = false;
}

uint64 FVoxelHydrologyWidthPort::GetAllocatedBytes() const
{ return VoxelGenerationAllocation::AllocatedBytes(Intervals) + VoxelGenerationAllocation::AllocatedBytes(ExpansionCounts); }
