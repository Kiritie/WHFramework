#include "Voxel/Rendering/VoxelCoveragePartition.h"

bool FVoxelCoverageRegion::IsValid() const
{
	return Min.X < Max.X && Min.Y < Max.Y && Min.Z < Max.Z;
}

void FVoxelCoverageEvaluationTask::Subtract(const FVoxelCoverageRegion& InRegion,
	const FVoxelCoverageRegion& InCut, TArray<FVoxelCoverageRegion>& OutPieces)
{
	FVoxelCoverageRegion Intersection;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Intersection.Min[Axis] = FMath::Max(InRegion.Min[Axis], InCut.Min[Axis]);
		Intersection.Max[Axis] = FMath::Min(InRegion.Max[Axis], InCut.Max[Axis]);
	}
	if (!Intersection.IsValid())
	{
		OutPieces.Add(InRegion);
		return;
	}
	FVoxelCoverageRegion Remaining = InRegion;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		if (Remaining.Min[Axis] < Intersection.Min[Axis])
		{
			FVoxelCoverageRegion Piece = Remaining;
			Piece.Max[Axis] = Intersection.Min[Axis];
			OutPieces.Add(Piece);
			Remaining.Min[Axis] = Intersection.Min[Axis];
		}
		if (Remaining.Max[Axis] > Intersection.Max[Axis])
		{
			FVoxelCoverageRegion Piece = Remaining;
			Piece.Min[Axis] = Intersection.Max[Axis];
			OutPieces.Add(Piece);
			Remaining.Max[Axis] = Intersection.Max[Axis];
		}
	}
}

bool FVoxelCoverageEvaluationTask::Begin(TConstArrayView<FVoxelCoverageRegion> InRequired,
	TConstArrayView<FVoxelCoverageRegion> InCover, FString& OutError)
{
	for (const FVoxelCoverageRegion& Region : InRequired)
	{
		if (!Region.IsValid())
		{
			OutError = TEXT("Invalid required coverage box");
			State = EVoxelCoverageEvaluation::Invalid;
			return false;
		}
	}
	for (const FVoxelCoverageRegion& Region : InCover)
	{
		if (!Region.IsValid())
		{
			OutError = TEXT("Invalid covering box");
			State = EVoxelCoverageEvaluation::Invalid;
			return false;
		}
	}
	Current.Reset();
	Current.Append(InRequired.GetData(), InRequired.Num());
	Cover.Reset();
	Cover.Append(InCover.GetData(), InCover.Num());
	Next.Reset();
	CutIndex = 0;
	RegionIndex = 0;
	Operations = 0;
	State = Current.IsEmpty() ? EVoxelCoverageEvaluation::Covered : EVoxelCoverageEvaluation::Pending;
	OutError.Reset();
	return true;
}

EVoxelCoverageEvaluation FVoxelCoverageEvaluationTask::Advance(const int32 InMaximumOperations,
	const TAtomic<bool>* InCancel)
{
	if (State != EVoxelCoverageEvaluation::Pending)
	{
		return State;
	}
	if (InCancel && InCancel->Load())
	{
		State = EVoxelCoverageEvaluation::Canceled;
		return State;
	}
	int32 Remaining = FMath::Max(0, InMaximumOperations);
	while (Remaining > 0)
	{
		if (InCancel && InCancel->Load())
		{
			State = EVoxelCoverageEvaluation::Canceled;
			return State;
		}
		if (Current.IsEmpty())
		{
			State = EVoxelCoverageEvaluation::Covered;
			return State;
		}
		if (CutIndex >= Cover.Num())
		{
			State = EVoxelCoverageEvaluation::Uncovered;
			return State;
		}
		if (RegionIndex == Current.Num())
		{
			Current = MoveTemp(Next);
			Next.Reset();
			RegionIndex = 0;
			++CutIndex;
			continue;
		}
		Subtract(Current[RegionIndex], Cover[CutIndex], Next);
		++RegionIndex;
		++Operations;
		--Remaining;
	}
	return State;
}
