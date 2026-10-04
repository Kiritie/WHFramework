#include "Voxel/Rendering/VoxelBoundaryRaster.h"

bool FVoxelBoundaryRaster::Validate(FString& OutError) const
{
	const int64 Width = int64(Max.X) - Min.X;
	const int64 Height = int64(Max.Y) - Min.Y;
	if (StepCells <= 0 || !FMath::IsPowerOfTwo(StepCells) || Width <= 0 || Height <= 0 ||
		Min.X % StepCells != 0 || Min.Y % StepCells != 0 || Width % StepCells != 0 || Height % StepCells != 0 ||
		Width / StepCells > MAX_int32 || Height / StepCells > MAX_int32 ||
		(Width / StepCells) * (Height / StepCells) != Cells.Num())
	{
		OutError = TEXT("Boundary raster must be complete and globally sample-aligned");
		return false;
	}
	OutError.Reset();
	return true;
}

const FVoxelBoundaryCell& FVoxelBoundaryRaster::Sample(const int32 InU, const int32 InV) const
{
	check(InU >= Min.X && InU < Max.X && InV >= Min.Y && InV < Max.Y);
	const int64 Width = (int64(Max.X) - Min.X) / StepCells;
	const int64 Index = (int64(InV) - Min.Y) / StepCells * Width + (int64(InU) - Min.X) / StepCells;
	return Cells[static_cast<int32>(Index)];
}

bool FVoxelHeightBoundary::Validate(FString& OutError) const
{
	const int64 Span = int64(Max) - Min;
	if (StepCells <= 0 || !FMath::IsPowerOfTwo(StepCells) || Span <= 0 || Min % StepCells != 0 ||
		Span % StepCells != 0 || Span / StepCells != Heights.Num() || Materials.Num() != Heights.Num())
	{
		OutError = TEXT("Height boundary must contain one rendered step value and material per interval");
		return false;
	}
	for (const double Height : Heights)
	{
		if (!FMath::IsFinite(Height))
		{
			OutError = TEXT("Non-finite rendered boundary height");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

FVoxelBoundaryFace FVoxelBoundaryRasterBuilder::MakeFace(const int32 InAxis, const double InPlane,
	const double InMinU, const double InMaxU, const double InMinV, const double InMaxV,
	const bool bInPositiveNormal, const uint16 InMaterial)
{
	const int32 UAxis = InAxis == 0 ? 1 : 0;
	const int32 VAxis = InAxis == 2 ? 1 : 2;
	FVoxelBoundaryFace Face;
	Face.Material = InMaterial;
	Face.Normal = FVector(0.0);
	Face.Normal[InAxis] = bInPositiveNormal ? 1.0 : -1.0;
	const double UValues[4] = {InMinU, InMaxU, InMaxU, InMinU};
	const double VValues[4] = {InMinV, InMinV, InMaxV, InMaxV};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Face.Vertices[Index] = FVector(0.0);
		Face.Vertices[Index][InAxis] = InPlane;
		Face.Vertices[Index][UAxis] = UValues[Index];
		Face.Vertices[Index][VAxis] = VValues[Index];
	}
	const bool bNaturalPositive = InAxis != 1;
	Face.bReverseWinding = bNaturalPositive != bInPositiveNormal;
	return Face;
}

bool FVoxelBoundaryRasterBuilder::BuildVolume(const FVoxelBoundaryRaster& InNegative,
	const FVoxelBoundaryRaster& InPositive, const int32 InAxis, const double InPlaneCells,
	const int32 InMaximumFaces, TArray<FVoxelBoundaryFace>& OutFaces, FString& OutError)
{
	if (InAxis < 0 || InAxis > 2 || !FMath::IsFinite(InPlaneCells) || InMaximumFaces < 0)
	{
		OutError = TEXT("Invalid volume boundary plane or output limit");
		return false;
	}
	if (!InNegative.Validate(OutError) || !InPositive.Validate(OutError))
	{
		return false;
	}
	const FIntPoint Min(FMath::Max(InNegative.Min.X, InPositive.Min.X), FMath::Max(InNegative.Min.Y, InPositive.Min.Y));
	const FIntPoint Max(FMath::Min(InNegative.Max.X, InPositive.Max.X), FMath::Min(InNegative.Max.Y, InPositive.Max.Y));
	if (Min.X >= Max.X || Min.Y >= Max.Y)
	{
		OutError = TEXT("Boundary rasters do not share a non-empty interface");
		return false;
	}
	const int32 Step = FMath::Min(InNegative.StepCells, InPositive.StepCells);
	TArray<FVoxelBoundaryFace> Candidate;
	for (int64 V = Min.Y; V < Max.Y; V += Step)
	{
		for (int64 U = Min.X; U < Max.X; U += Step)
		{
			const FVoxelBoundaryCell& Negative = InNegative.Sample(static_cast<int32>(U), static_cast<int32>(V));
			const FVoxelBoundaryCell& Positive = InPositive.Sample(static_cast<int32>(U), static_cast<int32>(V));
			if (Negative.bFilled == Positive.bFilled)
			{
				continue;
			}
			if (Candidate.Num() == InMaximumFaces)
			{
				OutError = TEXT("Boundary shard exceeds the face limit; split the job without changing sampling");
				return false;
			}
			Candidate.Add(MakeFace(InAxis, InPlaneCells, double(U), double(FMath::Min<int64>(U + Step, Max.X)),
				double(V), double(FMath::Min<int64>(V + Step, Max.Y)), Negative.bFilled,
				Negative.bFilled ? Negative.Material : Positive.Material));
		}
	}
	OutFaces = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FVoxelBoundaryRasterBuilder::BuildHeightfield(const FVoxelHeightBoundary& InNegative,
	const FVoxelHeightBoundary& InPositive, const int32 InAxis, const double InPlaneCells,
	const int32 InMaximumFaces, TArray<FVoxelBoundaryFace>& OutFaces, FString& OutError)
{
	if (InAxis < 0 || InAxis > 1 || !FMath::IsFinite(InPlaneCells) || InMaximumFaces < 0)
	{
		OutError = TEXT("Invalid heightfield boundary plane or output limit");
		return false;
	}
	if (!InNegative.Validate(OutError) || !InPositive.Validate(OutError))
	{
		return false;
	}
	const int32 Min = FMath::Max(InNegative.Min, InPositive.Min);
	const int32 Max = FMath::Min(InNegative.Max, InPositive.Max);
	if (Min >= Max)
	{
		OutError = TEXT("Height profiles do not share a non-empty interface");
		return false;
	}
	const int32 Step = FMath::Min(InNegative.StepCells, InPositive.StepCells);
	TArray<FVoxelBoundaryFace> Candidate;
	for (int64 Position = Min; Position < Max; Position += Step)
	{
		const int32 NegativeIndex = static_cast<int32>((Position - InNegative.Min) / InNegative.StepCells);
		const int32 PositiveIndex = static_cast<int32>((Position - InPositive.Min) / InPositive.StepCells);
		const double NegativeHeight = InNegative.Heights[NegativeIndex];
		const double PositiveHeight = InPositive.Heights[PositiveIndex];
		if (NegativeHeight == PositiveHeight)
		{
			continue;
		}
		if (Candidate.Num() == InMaximumFaces)
		{
			OutError = TEXT("Heightfield boundary shard exceeds the face limit");
			return false;
		}
		const bool bNegativeHigher = NegativeHeight > PositiveHeight;
		Candidate.Add(MakeFace(InAxis, InPlaneCells, double(Position), double(FMath::Min<int64>(Position + Step, Max)),
			FMath::Min(NegativeHeight, PositiveHeight), FMath::Max(NegativeHeight, PositiveHeight), bNegativeHigher,
			bNegativeHigher ? InNegative.Materials[NegativeIndex] : InPositive.Materials[PositiveIndex]));
	}
	OutFaces = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}
