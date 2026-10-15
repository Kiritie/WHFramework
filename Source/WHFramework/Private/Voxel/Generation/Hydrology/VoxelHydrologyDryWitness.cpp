#include "Voxel/Generation/Hydrology/VoxelHydrologyDryWitness.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
    constexpr int32 HydrologyWitnessMaximumNodes = 65536;
    constexpr int32 HydrologyWitnessProbeRings = 11;
    const FIntPoint HydrologyWitnessDirections[] = {
        FIntPoint(-1, 0), FIntPoint(1, 0), FIntPoint(0, -1), FIntPoint(0, 1),
        FIntPoint(-1, -1), FIntPoint(1, -1), FIntPoint(-1, 1), FIntPoint(1, 1)};

    int32 HydrologyWitnessCapacity(int32 InPrevious)
    {
        return FMath::Min(HydrologyWitnessMaximumNodes, FMath::Max(16, InPrevious * 2));
    }

    EVoxelHydrologyDryWitnessStep HydrologyWitnessFailure(const FString& InError)
    {
        if (InError == TEXT("Canceled")) return EVoxelHydrologyDryWitnessStep::Canceled;
        if (InError.StartsWith(TEXT("CapacityBlocked"))) return EVoxelHydrologyDryWitnessStep::CapacityBlocked;
        return EVoxelHydrologyDryWitnessStep::Failed;
    }
}

bool FVoxelHydrologyDryWitness::Initialize(const FIntPoint& InSeed, const FIntPoint& InMinimum,
    const FIntPoint& InMaximumInclusive, FString& OutError)
{
    if (Phase != EPhase::None || InMinimum.X > InMaximumInclusive.X || InMinimum.Y > InMaximumInclusive.Y ||
        InSeed.X < InMinimum.X || InSeed.X > InMaximumInclusive.X || InSeed.Y < InMinimum.Y || InSeed.Y > InMaximumInclusive.Y)
    {
        OutError = TEXT("Invalid dry drainage witness initialization");
        return false;
    }
    Seed.Cell = InSeed;
    Minimum = InMinimum;
    Maximum = InMaximumInclusive;
    Phase = EPhase::ReadSeed;
    OutError.Reset();
    return true;
}

bool FVoxelHydrologyDryWitness::ValidateSample(const FVoxelHydrologySharedCell& InSample, FString& OutError) const
{
    if (InSample.bHasCertifiedDryOutlet && (InSample.IsReceiver() || InSample.bHasLocalOutlet))
    {
        OutError = TEXT("Invalid mixed wet/local and certified dry witness");
        return false;
    }
    if (InSample.GroundZ < -32768 || InSample.GroundZ > 32768 || InSample.RainWeight > 10000 ||
        (InSample.IsReceiver() && (InSample.ReceiverWaterZ < -32768 || InSample.ReceiverWaterZ > 32768 ||
            InSample.GroundZ >= InSample.ReceiverWaterZ || (InSample.ReceiverId.High == 0 && InSample.ReceiverId.Low == 0))) ||
        (InSample.bHasLocalOutlet && (InSample.IsReceiver() || InSample.LocalOutletBarrier < InSample.GroundZ ||
            InSample.LocalOutletBarrier < InSample.LocalOutletWaterZ || InSample.LocalOutletBarrier > 32768 ||
            InSample.LocalOutletWaterZ < -32768 || InSample.LocalOutletWaterZ > 32768 ||
            (InSample.LocalOutletId.High == 0 && InSample.LocalOutletId.Low == 0))))
    {
        OutError = TEXT("Invalid certified cell in dry drainage witness");
        return false;
    }
    return true;
}

bool FVoxelHydrologyDryWitness::EventLess(const FEvent& InLeft, const FEvent& InRight) const
{
    const FIntPoint Left = Nodes[InLeft.Index].Sample.Cell;
    const FIntPoint Right = Nodes[InRight.Index].Sample.Cell;
    const int64 LeftScore = int64(InLeft.Distance) + FMath::Abs(int64(Left.X) - Target.X) + FMath::Abs(int64(Left.Y) - Target.Y);
    const int64 RightScore = int64(InRight.Distance) + FMath::Abs(int64(Right.X) - Target.X) + FMath::Abs(int64(Right.Y) - Target.Y);
    if (LeftScore != RightScore) return LeftScore < RightScore;
    if (InLeft.Distance != InRight.Distance) return InLeft.Distance > InRight.Distance;
    return Left.X != Right.X ? Left.X < Right.X : Left.Y < Right.Y;
}

bool FVoxelHydrologyDryWitness::Push(const FEvent& InEvent, FString& OutError)
{
    if (Frontier.Num() == Frontier.Max() && !VoxelGenerationAllocation::RetainArray(Frontier,
        FMath::Max(16, Frontier.Max() * 2), OutError)) return false;
    int32 Cursor = Frontier.Add(InEvent);
    while (Cursor > 0)
    {
        const int32 Parent = (Cursor - 1) / 2;
        if (!EventLess(Frontier[Cursor], Frontier[Parent])) break;
        Swap(Frontier[Cursor], Frontier[Parent]);
        Cursor = Parent;
    }
    return true;
}

FVoxelHydrologyDryWitness::FEvent FVoxelHydrologyDryWitness::Pop()
{
    const FEvent Result = Frontier[0];
    Frontier[0] = Frontier.Last();
    Frontier.RemoveAt(Frontier.Num() - 1, 1, EAllowShrinking::No);
    int32 Cursor = 0;
    while (int64(Cursor) * 2 + 1 < Frontier.Num())
    {
        int32 Child = Cursor * 2 + 1;
        if (Child + 1 < Frontier.Num() && EventLess(Frontier[Child + 1], Frontier[Child])) ++Child;
        if (!EventLess(Frontier[Child], Frontier[Cursor])) break;
        Swap(Frontier[Child], Frontier[Cursor]);
        Cursor = Child;
    }
    return Result;
}

bool FVoxelHydrologyDryWitness::AddNode(const FVoxelHydrologySharedCell& InSample, int32 InDistance, FString& OutError)
{
    if (Nodes.Num() == Nodes.Max() && !VoxelGenerationAllocation::RetainArray(Nodes,
        HydrologyWitnessCapacity(Nodes.Max()), OutError)) return false;
    if (ByCell.Num() == MapCapacity)
    {
        const int32 Capacity = HydrologyWitnessCapacity(MapCapacity);
        if (!VoxelGenerationAllocation::RetainMap(ByCell, Capacity, OutError)) return false;
        MapCapacity = Capacity;
    }
    if (Frontier.Num() == Frontier.Max() && !VoxelGenerationAllocation::RetainArray(Frontier,
        FMath::Max(16, Frontier.Max() * 2), OutError)) return false;
    const int32 Index = Nodes.Add(FNode{InSample, InDistance, false});
    ByCell.Add(InSample.Cell, Index);
    return Push(FEvent{Index, InDistance}, OutError);
}

EVoxelHydrologyDryWitnessStep FVoxelHydrologyDryWitness::Advance(FReadCell InReadCell, FWetHint InWetHint,
    int32 InWorkBudget, FString& OutError, const TAtomic<bool>* InCancel)
{
    const EPhase PreviousPhase = Phase;
    ON_SCOPE_EXIT
    {
        if (Phase != PreviousPhase && (Phase == EPhase::Proven || Phase == EPhase::Unproven))
        {
            static TAtomic<uint64> NextDiagnosticMilliseconds(0);
            const uint64 Now = uint64(FPlatformTime::Seconds() * 1000.0);
            uint64 Expected = NextDiagnosticMilliseconds.Load();
            if (Now >= Expected && NextDiagnosticMilliseconds.CompareExchange(Expected, Now + 5000))
            {
                UE_LOG(LogTemp, Display,
                    TEXT("Voxel dry witness outcome: seed=(%d,%d) ground=%d target=(%d,%d) proven=%d probes=%d nodes=%d frontier=%d fixedSeed=%d nodeLimit=%d bytes=%llu"),
                    Seed.Cell.X, Seed.Cell.Y, Seed.GroundZ, Target.X, Target.Y, Phase == EPhase::Proven,
                    ProbeIndex, Nodes.Num(), Frontier.Num(), Seed.IsReceiver(), HydrologyWitnessMaximumNodes, GetAllocatedBytes());
            }
        }
    };
    OutError.Reset();
    if (Phase == EPhase::None)
    {
        OutError = TEXT("Dry drainage witness not initialized");
        return EVoxelHydrologyDryWitnessStep::Failed;
    }
    if (InCancel && InCancel->Load())
    {
        OutError = TEXT("Canceled");
        return EVoxelHydrologyDryWitnessStep::Canceled;
    }
    if (Phase == EPhase::Proven) return EVoxelHydrologyDryWitnessStep::Proven;
    if (Phase == EPhase::Unproven) return EVoxelHydrologyDryWitnessStep::Unproven;
    while (InWorkBudget-- > 0)
    {
        if (InCancel && InCancel->Load())
        {
            OutError = TEXT("Canceled");
            return EVoxelHydrologyDryWitnessStep::Canceled;
        }
        if (Phase == EPhase::ReadSeed)
        {
            FVoxelHydrologySharedCell Value;
            const auto Read = InReadCell(Seed.Cell, Value, OutError);
            if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyDryWitnessStep::NeedInput;
            if (Read != EVoxelHydrologyClosureRead::Ready || !ValidateSample(Value, OutError)) return HydrologyWitnessFailure(OutError);
            Value.Cell = Seed.Cell;
            Seed = Value;
            if (Seed.IsReceiver())
            {
                Phase = EPhase::Unproven;
                return EVoxelHydrologyDryWitnessStep::Unproven;
            }
            Phase = EPhase::FindTarget;
        }
        else if (Phase == EPhase::FindTarget)
        {
            if (ProbeIndex == HydrologyWitnessProbeRings * UE_ARRAY_COUNT(HydrologyWitnessDirections))
            {
                Phase = EPhase::Unproven;
                return EVoxelHydrologyDryWitnessStep::Unproven;
            }
            const int64 Radius = int64(64) << (ProbeIndex / UE_ARRAY_COUNT(HydrologyWitnessDirections));
            const FIntPoint Direction = HydrologyWitnessDirections[ProbeIndex++ % UE_ARRAY_COUNT(HydrologyWitnessDirections)];
            const int64 X = int64(Seed.Cell.X) + Radius * Direction.X;
            const int64 Y = int64(Seed.Cell.Y) + Radius * Direction.Y;
            if (X < Minimum.X || X > Maximum.X || Y < Minimum.Y || Y > Maximum.Y) continue;
            const FIntPoint Candidate{int32(X), int32(Y)};
            if (!InWetHint(Candidate)) continue;
            Target = Candidate;
            if (!AddNode(Seed, 0, OutError)) return HydrologyWitnessFailure(OutError);
            Phase = EPhase::Search;
        }
        else if (Phase == EPhase::Search)
        {
            if (Current == INDEX_NONE)
            {
                if (Frontier.IsEmpty() || Nodes.Num() >= HydrologyWitnessMaximumNodes)
                {
                    Phase = EPhase::Unproven;
                    return EVoxelHydrologyDryWitnessStep::Unproven;
                }
                const FEvent Event = Pop();
                if (Nodes[Event.Index].bClosed || Event.Distance != Nodes[Event.Index].Distance) continue;
                Nodes[Event.Index].bClosed = true;
                Current = Event.Index;
                Face = 0;
                // 本路径每格障碍均不高于种子；已认证干列接续的真实排水路径同样满足该界。
                if (Nodes[Current].Sample.IsReceiver() || Nodes[Current].Sample.bHasCertifiedDryOutlet)
                {
                    Seed.SpillZ = Seed.GroundZ;
                    Seed.bIncluded = true;
                    Seed.Parent = INDEX_NONE;
                    Seed.bParentIsLocalOutlet = false;
                    Phase = EPhase::Proven;
                    return EVoxelHydrologyDryWitnessStep::Proven;
                }
                continue;
            }
            const FIntPoint Origin = Nodes[Current].Sample.Cell;
            const FIntPoint Direction = HydrologyWitnessDirections[Face];
            const int64 X = int64(Origin.X) + Direction.X;
            const int64 Y = int64(Origin.Y) + Direction.Y;
            if (X >= Minimum.X && X <= Maximum.X && Y >= Minimum.Y && Y <= Maximum.Y)
            {
                const FIntPoint Position{int32(X), int32(Y)};
                const int32 Distance = Nodes[Current].Distance + 1;
                if (const int32* Existing = ByCell.Find(Position))
                {
                    if (!Nodes[*Existing].bClosed && Distance < Nodes[*Existing].Distance)
                    {
                        if (!Push(FEvent{*Existing, Distance}, OutError)) return HydrologyWitnessFailure(OutError);
                        Nodes[*Existing].Distance = Distance;
                    }
                }
                else
                {
                    if (Nodes.Num() >= HydrologyWitnessMaximumNodes)
                    {
                        Phase = EPhase::Unproven;
                        return EVoxelHydrologyDryWitnessStep::Unproven;
                    }
                    FVoxelHydrologySharedCell Value;
                    const auto Read = InReadCell(Position, Value, OutError);
                    if (Read == EVoxelHydrologyClosureRead::Pending) return EVoxelHydrologyDryWitnessStep::NeedInput;
                    if (Read != EVoxelHydrologyClosureRead::Ready || !ValidateSample(Value, OutError)) return HydrologyWitnessFailure(OutError);
                    Value.Cell = Position;
                    const int32 Barrier = Value.IsReceiver() ? Value.ReceiverWaterZ : Value.GroundZ;
                    if (Barrier <= Seed.GroundZ && !AddNode(Value, Distance, OutError)) return HydrologyWitnessFailure(OutError);
                }
            }
            if (++Face == 4) Current = INDEX_NONE;
        }
    }
    return EVoxelHydrologyDryWitnessStep::Yielded;
}

const FVoxelHydrologySharedCell& FVoxelHydrologyDryWitness::GetCertificate() const
{
    check(Phase == EPhase::Proven);
    return Seed;
}

uint64 FVoxelHydrologyDryWitness::GetAllocatedBytes() const
{
    return VoxelGenerationAllocation::AllocatedBytes(Nodes) + VoxelGenerationAllocation::AllocatedBytes(ByCell) +
        VoxelGenerationAllocation::AllocatedBytes(Frontier);
}

void FVoxelHydrologyDryWitness::FreeBacking()
{
    VoxelGenerationAllocation::FreeRetainedArray(Nodes);
    VoxelGenerationAllocation::FreeRetainedMap(ByCell);
    VoxelGenerationAllocation::FreeRetainedArray(Frontier);
    Seed = FVoxelHydrologySharedCell();
    Phase = EPhase::None;
    MapCapacity = ProbeIndex = Face = 0;
    Current = INDEX_NONE;
}
