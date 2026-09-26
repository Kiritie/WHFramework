#include "Voxel/Generation/Hydrology/VoxelHydrologyPlan.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	FVoxelRiverShapePoint ToShapePoint(const FVoxelRiverRoutePoint& InPoint)
	{
		FVoxelRiverShapePoint Result;
		Result.Position = InPoint.Position;
		Result.WaterZ = InPoint.WaterZ;
		Result.BedHalfWidth = InPoint.BedHalfWidth;
		Result.WaterHalfWidth = InPoint.WaterHalfWidth;
		Result.BankWidth = InPoint.BankWidth;
		Result.ShoreWidth = InPoint.ShoreWidth;
		Result.Depth = InPoint.Depth;
		Result.Accumulation = InPoint.Accumulation;
		return Result;
	}

	uint64 InterpolateAccumulation(const uint64 A, const uint64 B,
		const uint32 Numerator, const uint32 Denominator)
	{
		const uint64 Delta = A > B ? A - B : B - A;
		const uint64 Portion = Delta / Denominator * Numerator +
			(Delta % Denominator * Numerator) / Denominator;
		return B >= A ? A + Portion : A - Portion;
	}

	FVoxelRiverShapePoint InterpolateShapePoint(
		const FVoxelRiverShapePoint& A,
		const FVoxelRiverShapePoint& B,
		const int32 Numerator,
		const int32 Denominator)
	{
		auto Blend = [Numerator, Denominator](const int32 First, const int32 Second)
		{
			return First + static_cast<int32>(
				(static_cast<int64>(Second) - First) * Numerator / Denominator);
		};
		FVoxelRiverShapePoint Result;
		Result.Position.X = Blend(A.Position.X, B.Position.X);
		Result.Position.Y = Blend(A.Position.Y, B.Position.Y);
		Result.WaterZ = Blend(A.WaterZ, B.WaterZ);
		Result.BedHalfWidth = Blend(A.BedHalfWidth, B.BedHalfWidth);
		Result.WaterHalfWidth = Blend(A.WaterHalfWidth, B.WaterHalfWidth);
		Result.BankWidth = Blend(A.BankWidth, B.BankWidth);
		Result.ShoreWidth = Blend(A.ShoreWidth, B.ShoreWidth);
		Result.Depth = Blend(A.Depth, B.Depth);
		Result.Accumulation = InterpolateAccumulation(A.Accumulation,
			B.Accumulation, Numerator, Denominator);
		return Result;
	}

	void BuildShapeRoute(const TArray<FVoxelRiverRoutePoint>& InRaw,
		const FIntPoint& InCoreMin, const FIntPoint& InCoreMax,
		const int32 InPasses, TArray<FVoxelRiverShapePoint>& OutShape)
	{
		OutShape.Reset();
		auto InsideCore = [&InCoreMin, &InCoreMax](const FIntPoint& Position)
		{
			return Position.X >= InCoreMin.X && Position.Y >= InCoreMin.Y &&
				Position.X < InCoreMax.X && Position.Y < InCoreMax.Y;
		};
		for (int32 Index = 0; Index < InRaw.Num(); ++Index)
		{
			FVoxelRiverShapePoint Point = ToShapePoint(InRaw[Index]);
			if (Index > 0 && InsideCore(InRaw[Index].Position) !=
				InsideCore(InRaw[Index - 1].Position))
			{
				FVoxelRiverShapePoint Boundary = InterpolateShapePoint(
					ToShapePoint(InRaw[Index - 1]), Point, 1, 2);
				Boundary.bAnchor = true;
				if (Boundary.Position != OutShape.Last().Position &&
					Boundary.Position != Point.Position)
				{
					OutShape.Add(MoveTemp(Boundary));
				}
			}
			Point.bAnchor = Index == 0 || Index + 1 == InRaw.Num();
			OutShape.Add(MoveTemp(Point));
		}
		for (int32 Pass = 0; Pass < InPasses && OutShape.Num() >= 2; ++Pass)
		{
			TArray<FVoxelRiverShapePoint> Next;
			Next.Reserve(OutShape.Num() * 2);
			Next.Add(OutShape[0]);
			for (int32 Index = 0; Index + 1 < OutShape.Num(); ++Index)
			{
				for (const int32 Fraction : {1, 3})
				{
					FVoxelRiverShapePoint Point = InterpolateShapePoint(
						OutShape[Index], OutShape[Index + 1], Fraction, 4);
					if (Point.Position != Next.Last().Position) Next.Add(MoveTemp(Point));
				}
				if (OutShape[Index + 1].bAnchor && Index + 2 < OutShape.Num())
				{
					if (OutShape[Index + 1].Position == Next.Last().Position)
					{
						Next.Last().bAnchor = true;
					}
					else
					{
						Next.Add(OutShape[Index + 1]);
					}
				}
			}
			if (Next.Last().Position != OutShape.Last().Position)
			{
				Next.Add(OutShape.Last());
			}
			else
			{
				Next.Last().bAnchor = true;
			}
			OutShape = MoveTemp(Next);
		}
		if (OutShape.Num() < 3) return;
		const TArray<FVoxelRiverShapePoint> Original = OutShape;
		auto Smooth = [](const int32 A, const int32 B, const int32 C)
		{
			return static_cast<int32>((static_cast<int64>(A) +
				static_cast<int64>(B) * 2 + C) / 4);
		};
		for (int32 Index = 1; Index + 1 < OutShape.Num(); ++Index)
		{
			OutShape[Index].BedHalfWidth = Smooth(Original[Index - 1].BedHalfWidth,
				Original[Index].BedHalfWidth, Original[Index + 1].BedHalfWidth);
			OutShape[Index].WaterHalfWidth = Smooth(Original[Index - 1].WaterHalfWidth,
				Original[Index].WaterHalfWidth, Original[Index + 1].WaterHalfWidth);
			OutShape[Index].Depth = Smooth(Original[Index - 1].Depth,
				Original[Index].Depth, Original[Index + 1].Depth);
		}
	}

	int32 MeanderTangentQ10(const int32 InDegrees)
	{
		static constexpr int32 Values[] = {0, 90, 181, 274, 373, 477,
			591, 717, 859, 1024, 1221, 1462, 1774, 2196, 2813, 3822, 5807};
		const int32 Degrees = FMath::Clamp(InDegrees, 1, 80);
		const int32 Bucket = Degrees / 5;
		if (Bucket >= 16) return Values[16];
		return Values[Bucket] +
			(Values[Bucket + 1] - Values[Bucket]) * (Degrees % 5) / 5;
	}

	bool IsMeanderAngleWithinLimit(const FIntPoint& InPrevious,
		const FIntPoint& InCurrent, const FIntPoint& InNext,
		const int32 InTangentQ10)
	{
		const FIntPoint A = InCurrent - InPrevious;
		const FIntPoint B = InNext - InCurrent;
		const int64 Dot = static_cast<int64>(A.X) * B.X +
			static_cast<int64>(A.Y) * B.Y;
		const int64 Cross = FMath::Abs(static_cast<int64>(A.X) * B.Y -
			static_cast<int64>(A.Y) * B.X);
		return Dot > 0 && Cross * 1024 <= Dot * InTangentQ10;
	}

	void BuildMeanderRoute(const TArray<FVoxelRiverShapePoint>& InShape,
		const int32 InSeed, const int32 InStrength, const int32 InFrequency,
		const int32 InOctaves, const int32 InMaxAngle,
		TArray<FVoxelRiverMeanderPoint>& OutPoints)
	{
		OutPoints.Reset();
		OutPoints.Reserve(InShape.Num());
		if (InShape.Num() < 2) return;
		TArray<int32> DistanceToAnchor;
		DistanceToAnchor.SetNum(InShape.Num());
		int32 Distance = InShape.Num();
		for (int32 Index = 0; Index < InShape.Num(); ++Index)
		{
			Distance = InShape[Index].bAnchor ? 0 : Distance + 1;
			DistanceToAnchor[Index] = Distance;
		}
		Distance = InShape.Num();
		for (int32 Index = InShape.Num() - 1; Index >= 0; --Index)
		{
			Distance = InShape[Index].bAnchor ? 0 : Distance + 1;
			DistanceToAnchor[Index] = FMath::Min(DistanceToAnchor[Index], Distance);
		}
		for (int32 Index = 0; Index < InShape.Num(); ++Index)
		{
			const FVoxelRiverShapePoint& Source = InShape[Index];
			FVoxelRiverMeanderPoint& Result = OutPoints.AddDefaulted_GetRef();
			Result.Position = Source.Position;
			Result.bAnchor = Source.bAnchor;
			Result.WaterZ = Source.WaterZ;
			Result.BedHalfWidth = Source.BedHalfWidth;
			Result.WaterHalfWidth = Source.WaterHalfWidth;
			Result.BankWidth = Source.BankWidth;
			Result.ShoreWidth = Source.ShoreWidth;
			Result.Depth = Source.Depth;
			Result.Accumulation = Source.Accumulation;
			if (Source.bAnchor || InStrength == 0)
			{
				continue;
			}
			const FIntPoint Tangent = InShape[Index + 1].Position -
				InShape[Index - 1].Position;
			const int64 Length = FMath::Abs(static_cast<int64>(Tangent.X)) +
				FMath::Abs(static_cast<int64>(Tangent.Y));
			if (Length == 0) continue;
			int64 Noise = 0;
			int64 Weight = 0;
			for (int32 Octave = 0; Octave < InOctaves; ++Octave)
			{
				const int32 Period = FMath::Max(8, InFrequency >> Octave);
				const int32 OctaveWeight = 1 << (InOctaves - Octave - 1);
				Noise += static_cast<int64>(VoxelGeneration::Noise2D(InSeed,
					Source.Position.X, Source.Position.Y, Period,
					0x52495645524D4541ull + Octave)) * OctaveWeight;
				Weight += OctaveWeight;
			}
			// Keep the first two shape samples on each side of a fixed
			// connection aligned with its drainage segment. At voxel scale,
			// moving either sample by one cell can make a 45-degree corner.
			const int32 Taper = FMath::Clamp(
				(DistanceToAnchor[Index] - 2) * 256, 0, 1024);
			const int32 Offset = static_cast<int32>(
				Noise * InStrength * Taper / FMath::Max<int64>(1,
					Weight * 32768 * 1024));
			Result.Position.X += static_cast<int32>(
				-static_cast<int64>(Tangent.Y) * Offset / Length);
			Result.Position.Y += static_cast<int32>(
				static_cast<int64>(Tangent.X) * Offset / Length);
			Result.MeanderOffset = Offset;
		}
		const int32 TangentQ10 = MeanderTangentQ10(InMaxAngle);
		for (int32 Pass = 0; Pass < 24; ++Pass)
		{
			bool bChanged = false;
			for (int32 Index = 1; Index + 1 < OutPoints.Num(); ++Index)
			{
				if (OutPoints[Index].bAnchor) continue;
				if (IsMeanderAngleWithinLimit(OutPoints[Index - 1].Position,
					OutPoints[Index].Position, OutPoints[Index + 1].Position,
					TangentQ10)) continue;
				const FIntPoint Midpoint(
					static_cast<int32>((static_cast<int64>(OutPoints[Index - 1].Position.X) +
						OutPoints[Index + 1].Position.X) / 2),
					static_cast<int32>((static_cast<int64>(OutPoints[Index - 1].Position.Y) +
						OutPoints[Index + 1].Position.Y) / 2));
				if (Midpoint != OutPoints[Index].Position)
				{
					OutPoints[Index].Position = Midpoint;
					bChanged = true;
				}
			}
			if (!bChanged) break;
		}
		for (int32 Index = OutPoints.Num() - 2; Index > 0; --Index)
		{
			if (OutPoints[Index].bAnchor) continue;
			if (OutPoints[Index].Position == OutPoints[Index - 1].Position ||
				OutPoints[Index].Position == OutPoints[Index + 1].Position)
			{
				OutPoints.RemoveAt(Index);
			}
		}
		for (int32 Index = 1; Index + 1 < OutPoints.Num();)
		{
			if (IsMeanderAngleWithinLimit(OutPoints[Index - 1].Position,
				OutPoints[Index].Position, OutPoints[Index + 1].Position,
				TangentQ10))
			{
				++Index;
				continue;
			}
			if (OutPoints[Index].bAnchor)
			{
				++Index;
				continue;
			}
			OutPoints.RemoveAt(Index);
			Index = FMath::Max(1, Index - 1);
		}
		for (int32 Index = 1; Index + 1 < OutPoints.Num(); ++Index)
		{
			const FIntPoint A = OutPoints[Index].Position - OutPoints[Index - 1].Position;
			const FIntPoint B = OutPoints[Index + 1].Position - OutPoints[Index].Position;
			const int64 Cross = static_cast<int64>(A.X) * B.Y -
				static_cast<int64>(A.Y) * B.X;
			OutPoints[Index].Curvature = Cross > 0 ? 1 : Cross < 0 ? -1 : 0;
		}
	}

	int32 ResolveRiverInfluenceRadius(
		const FVoxelRiverMeanderPoint& InPoint)
	{
		return InPoint.WaterHalfWidth + InPoint.BankWidth +
			InPoint.ShoreWidth;
	}
}

void FVoxelHydrologyPlan::Finalize()
{
	LakeWaterByCellOrigin.Reset();
	RiverSegmentsByHydrologyCell.Reset();
	for (FVoxelRiverRoute& River : Rivers)
	{
		BuildShapeRoute(River.Points, CoreMin, CoreMax,
			RiverShapeSmoothingPasses,
			River.ShapePoints);
		BuildMeanderRoute(River.ShapePoints, RiverSeed,
			RiverMeanderStrength, RiverMeanderFrequency,
			RiverMeanderOctaves, RiverMaxMeanderAngle,
			River.MeanderPoints);
		if (!River.MeanderPoints.IsEmpty())
		{
			River.Min = River.MeanderPoints[0].Position;
			River.Max = River.Min;
			for (const FVoxelRiverMeanderPoint& Point : River.MeanderPoints)
			{
				River.Min.X = FMath::Min(River.Min.X, Point.Position.X);
				River.Min.Y = FMath::Min(River.Min.Y, Point.Position.Y);
				River.Max.X = FMath::Max(River.Max.X, Point.Position.X);
				River.Max.Y = FMath::Max(River.Max.Y, Point.Position.Y);
			}
		}
	}

	for (const FVoxelLakePlan& Lake :
		Lakes)
	{
		for (const FIntPoint& Cell :
			Lake.Cells)
		{
			int32& WaterZ =
				LakeWaterByCellOrigin.
					FindOrAdd(
						Cell,
						MIN_int32);

			WaterZ =
				FMath::Max(
					WaterZ,
					Lake.WaterZ);
		}
	}

	if (Grid.CellSize <= 0)
	{
		return;
	}

	for (int32 RiverIndex = 0;
		RiverIndex < Rivers.Num();
		++RiverIndex)
	{
		const FVoxelRiverRoute& River =
			Rivers[RiverIndex];

		for (int32 PointIndex = 1;
			PointIndex <
				River.MeanderPoints.Num();
			++PointIndex)
		{
			const FVoxelRiverMeanderPoint& A =
				River.MeanderPoints[
					PointIndex - 1];

			const FVoxelRiverMeanderPoint& B =
				River.MeanderPoints[
					PointIndex];

			const int32 Radius =
				FMath::Max(
					ResolveRiverInfluenceRadius(A),
					ResolveRiverInfluenceRadius(B));

			const int32 MinX =
				FMath::Min(
					A.Position.X,
					B.Position.X) -
				Radius;

			const int32 MinY =
				FMath::Min(
					A.Position.Y,
					B.Position.Y) -
				Radius;

			const int32 MaxX =
				FMath::Max(
					A.Position.X,
					B.Position.X) +
				Radius;

			const int32 MaxY =
				FMath::Max(
					A.Position.Y,
					B.Position.Y) +
				Radius;

			const int32 MinCellX =
				VoxelGeneration::FloorDivide(
					MinX,
					Grid.CellSize);

			const int32 MinCellY =
				VoxelGeneration::FloorDivide(
					MinY,
					Grid.CellSize);

			const int32 MaxCellX =
				VoxelGeneration::FloorDivide(
					MaxX,
					Grid.CellSize);

			const int32 MaxCellY =
				VoxelGeneration::FloorDivide(
					MaxY,
					Grid.CellSize);

			for (int32 CellY = MinCellY;
				CellY <= MaxCellY;
				++CellY)
			{
				for (int32 CellX = MinCellX;
					CellX <= MaxCellX;
					++CellX)
				{
					RiverSegmentsByHydrologyCell.
						FindOrAdd(
							FIntPoint(
								CellX,
								CellY)).
						Add({
							RiverIndex,
							PointIndex
						});
				}
			}
		}
	}
}

bool FVoxelHydrologyPlan::ValidateRiverRoutes(FString& OutError) const
{
	const int32 TangentQ10 = MeanderTangentQ10(RiverMaxMeanderAngle);
	for (const FVoxelRiverRoute& River : Rivers)
	{
		if (River.ShapePoints.Num() < 2 || River.MeanderPoints.Num() < 2)
		{
			OutError = TEXT("River has no valid shape route");
			return false;
		}
		for (int32 Index = 0; Index < River.MeanderPoints.Num(); ++Index)
		{
			const FVoxelRiverMeanderPoint& Point = River.MeanderPoints[Index];
			if (Point.BedHalfWidth < 1 || Point.WaterHalfWidth < Point.BedHalfWidth ||
				Point.BankWidth < 1 || Point.ShoreWidth < 1 || Point.Depth < 1)
			{
				OutError = TEXT("River shape contains an invalid width or depth");
				return false;
			}
			if (Index == 0) continue;
			const FVoxelRiverMeanderPoint& Previous = River.MeanderPoints[Index - 1];
			if (Point.Position == Previous.Position || Point.WaterZ > Previous.WaterZ + 1)
			{
				OutError = TEXT("River shape has a zero segment or uphill discontinuity");
				return false;
			}
			if (Index > 1 && !IsMeanderAngleWithinLimit(
				River.MeanderPoints[Index - 2].Position, Previous.Position,
				Point.Position, TangentQ10))
			{
				const FIntPoint Before = River.MeanderPoints[Index - 2].Position;
				OutError = FString::Printf(TEXT("River meander exceeds %d degrees: (%d,%d) -> (%d,%d) -> (%d,%d), anchor=%d"),
					RiverMaxMeanderAngle, Before.X, Before.Y,
					Previous.Position.X, Previous.Position.Y,
					Point.Position.X, Point.Position.Y,
					Previous.bAnchor ? 1 : 0);
				return false;
			}
		}
	}
	OutError.Reset();
	return true;
}

bool FVoxelHydrologyPlan::Sample(
	const int32 InWorldX,
	const int32 InWorldY,
	const int32 InOriginalGround,
	FVoxelHydrologyInfluence& OutInfluence) const
{
	if (Grid.CellSize <= 0 ||
		Grid.Width <= 0 ||
		Grid.Height <= 0)
	{
		return false;
	}

	const FIntPoint WorldHydrologyCell(
		VoxelGeneration::FloorDivide(
			InWorldX,
			Grid.CellSize),
		VoxelGeneration::FloorDivide(
			InWorldY,
			Grid.CellSize));

	const FIntPoint LocalCell =
		WorldHydrologyCell -
		Grid.WorldMinCell;

	if (LocalCell.X < 0 ||
		LocalCell.Y < 0 ||
		LocalCell.X >= Grid.Width ||
		LocalCell.Y >= Grid.Height)
	{
		return false;
	}

	FVoxelHydrologyInfluence Result;

	const uint32 GridIndex =
		Grid.ToIndex(
			LocalCell.X,
			LocalCell.Y);

	if (const int32* OceanPlane =
		Grid.OceanOutlets.Find(
			GridIndex))
	{
		Result.SurfaceWaterZ =
			*OceanPlane;

		Result.GroundOverrideZ =
			FMath::Min(
				InOriginalGround,
				*OceanPlane - 1);

		Result.bOcean = true;
	}
	else
	{
		static const FIntPoint Neighbors[] =
		{
			FIntPoint(-1, 0),
			FIntPoint(1, 0),
			FIntPoint(0, -1),
			FIntPoint(0, 1)
		};

		for (const FIntPoint Offset :
			Neighbors)
		{
			const FIntPoint Neighbor =
				LocalCell +
				Offset;

			if (Neighbor.X >= 0 &&
				Neighbor.Y >= 0 &&
				Neighbor.X < Grid.Width &&
				Neighbor.Y < Grid.Height &&
				Grid.OceanOutlets.Contains(
					Grid.ToIndex(
						Neighbor.X,
						Neighbor.Y)))
			{
				Result.bCoast = true;
				break;
			}
		}
	}

	const FIntPoint SampleCellOrigin(
		WorldHydrologyCell.X *
			Grid.CellSize,
		WorldHydrologyCell.Y *
			Grid.CellSize);

	if (const int32* LakeWaterZ =
		LakeWaterByCellOrigin.Find(
			SampleCellOrigin))
	{
		Result.SurfaceWaterZ =
			FMath::Max(
				Result.SurfaceWaterZ,
				*LakeWaterZ);

		Result.GroundOverrideZ =
			FMath::Min(
				InOriginalGround,
				*LakeWaterZ - 1);

		Result.bLake = true;
		Result.bOcean = false;
	}

	int32 BestDistance =
		MAX_int32;
	int32 BestBankExtent = 0;
	int32 BestShoreExtent = 0;
	FVoxelStableId BestRiverId;

	FVoxelRiverSection
		BestSection;

	bool bHasRiver = false;

	const TArray<FVoxelRiverSegmentRef>* Segments =
		RiverSegmentsByHydrologyCell.Find(
			WorldHydrologyCell);

	if (Segments)
	{
		const FIntPoint Sample(
			InWorldX,
			InWorldY);

		for (const FVoxelRiverSegmentRef& Ref :
			*Segments)
		{
			if (!Rivers.IsValidIndex(
					Ref.RiverIndex))
			{
				continue;
			}

			const FVoxelRiverRoute& River =
				Rivers[
					Ref.RiverIndex];

			if (Ref.ShapePointIndex <= 0 ||
				!River.MeanderPoints.IsValidIndex(
					Ref.ShapePointIndex))
			{
				continue;
			}

			const FVoxelRiverMeanderPoint& A =
				River.MeanderPoints[
					Ref.ShapePointIndex - 1];

			const FVoxelRiverMeanderPoint& B =
				River.MeanderPoints[
					Ref.ShapePointIndex];

			const int64 ABX =
				static_cast<int64>(
					B.Position.X) -
				A.Position.X;

			const int64 ABY =
				static_cast<int64>(
					B.Position.Y) -
				A.Position.Y;

			const int64 APX =
				static_cast<int64>(
					Sample.X) -
				A.Position.X;

			const int64 APY =
				static_cast<int64>(
					Sample.Y) -
				A.Position.Y;

			const int64 LengthSquared =
				ABX * ABX +
				ABY * ABY;

			const int32 AlphaQ16 =
				LengthSquared > 0
					? static_cast<int32>(
						FMath::Clamp<int64>(
							(APX * ABX +
							 APY * ABY) *
								65536 /
								LengthSquared,
							0,
							65536))
					: 0;

			const int64 ClosestXQ16 =
				static_cast<int64>(
					A.Position.X) *
					65536 +
				ABX *
					AlphaQ16;

			const int64 ClosestYQ16 =
				static_cast<int64>(
					A.Position.Y) *
					65536 +
				ABY *
					AlphaQ16;

			const int64 DX =
				FMath::Abs(
					static_cast<int64>(
						Sample.X) *
						65536 -
					ClosestXQ16);

			const int64 DY =
				FMath::Abs(
					static_cast<int64>(
						Sample.Y) *
						65536 -
					ClosestYQ16);

			const int32 Distance =
				static_cast<int32>(
					(FMath::Max(
						DX,
						DY) +
					 FMath::Min(
						DX,
						DY) *
						3 /
						8) /
					65536);

			if (Distance >=
				BestDistance)
			{
				continue;
			}

			const int32 WaterZ =
				A.WaterZ +
				static_cast<int32>(
					static_cast<int64>(
						B.WaterZ -
						A.WaterZ) *
					AlphaQ16 /
					65536);

			FVoxelRiverShape Shape;

			Shape.BedHalfWidth =
				A.BedHalfWidth +
				static_cast<int32>(
					static_cast<int64>(
						B.BedHalfWidth -
						A.BedHalfWidth) *
					AlphaQ16 /
					65536);
			Shape.WaterHalfWidth = A.WaterHalfWidth +
				static_cast<int32>(static_cast<int64>(
					B.WaterHalfWidth - A.WaterHalfWidth) * AlphaQ16 / 65536);

			Shape.Depth =
				A.Depth +
				static_cast<int32>(
					static_cast<int64>(
						B.Depth -
						A.Depth) *
					AlphaQ16 /
					65536);

			Shape.BankWidth = A.BankWidth +
				static_cast<int32>(static_cast<int64>(
					B.BankWidth - A.BankWidth) * AlphaQ16 / 65536);
			Shape.ShoreWidth = A.ShoreWidth +
				static_cast<int32>(static_cast<int64>(
					B.ShoreWidth - A.ShoreWidth) * AlphaQ16 / 65536);
			const int32 Curvature = AlphaQ16 < 32768 ? A.Curvature : B.Curvature;
			const int64 Side = ABX * APY - ABY * APX;
			if (Curvature != 0 && Side != 0)
			{
				if ((Curvature > 0 && Side < 0) ||
					(Curvature < 0 && Side > 0))
				{
					const int32 Shift = FMath::Min(Shape.ShoreWidth - 1,
						FMath::Max(1, Shape.BankWidth / 5));
					Shape.BankWidth += Shift;
					Shape.ShoreWidth -= Shift;
				}
				else
				{
					const int32 Shift = FMath::Min(Shape.BankWidth - 1,
						FMath::Max(1, Shape.BankWidth / 10));
					Shape.BankWidth -= Shift;
					Shape.ShoreWidth += Shift;
				}
			}

			Shape.MaxCutFill =
				FMath::Max(
					64,
					Shape.Depth * 4);

			FVoxelRiverSection Section;
			FString Error;

			if (VoxelHydrology::
				EvaluateRiverSection(
					InOriginalGround,
					WaterZ,
					Distance,
					Shape,
					Section,
					Error))
			{
				BestDistance =
					Distance;
				BestBankExtent = Shape.WaterHalfWidth + Shape.BankWidth;
				BestShoreExtent = BestBankExtent + Shape.ShoreWidth;
				BestRiverId = River.Id;

				BestSection =
					Section;

				bHasRiver = true;
			}
		}
	}
	if (bHasRiver)
	{
		Result.RiverId = BestRiverId;
		Result.RiverDistanceCells = BestDistance;
		Result.BankDistanceCells = FMath::Max(0, BestDistance - BestBankExtent);
		Result.FloodplainStrengthQ15 = BestDistance <= BestBankExtent ?
			32767 : static_cast<int32>(
				static_cast<int64>(FMath::Max(0, BestShoreExtent - BestDistance)) *
				32767 / FMath::Max(1, BestShoreExtent - BestBankExtent));
	}

	if (bHasRiver &&
		!Result.bOcean &&
		!Result.bLake)
	{
		Result.GroundOverrideZ =
			BestSection.GroundPlane;

		Result.SurfaceWaterZ =
			BestSection.bWet
				? BestSection.WaterPlane
				: MIN_int32;

		Result.bRiver =
			BestSection.bWet;
		Result.RiverZone = BestSection.Zone;
	}

	OutInfluence =
		Result;

	return
		Result.bOcean ||
		Result.bLake ||
		Result.bRiver ||
		Result.bCoast ||
		Result.GroundOverrideZ !=
			MIN_int32;
}

uint64 FVoxelHydrologyPlan::GetAllocatedBytes() const
{
	uint64 Bytes =
		Grid.GroundPlane.GetAllocatedSize() +
		Grid.RainWeight.GetAllocatedSize() +
		Grid.Allowed.GetAllocatedSize() +
		Grid.OceanOutlets.GetAllocatedSize() +
		Grid.CanonicalSinkOutlets.GetAllocatedSize() +
		Drainage.SpillPlane.GetAllocatedSize() +
		Drainage.Parent.GetAllocatedSize() +
		Drainage.SettledOrder.GetAllocatedSize() +
		Drainage.Accumulation.GetAllocatedSize() +
		Rivers.GetAllocatedSize() +
		Lakes.GetAllocatedSize() +
		LakeWaterByCellOrigin.GetAllocatedSize() +
		RiverSegmentsByHydrologyCell.GetAllocatedSize();

	for (const FVoxelRiverRoute& River :
		Rivers)
	{
		Bytes +=
			River.Points.GetAllocatedSize();
		Bytes += River.ShapePoints.GetAllocatedSize();
		Bytes += River.MeanderPoints.GetAllocatedSize();
	}

	for (const FVoxelLakePlan& Lake :
		Lakes)
	{
		Bytes +=
			Lake.Cells.GetAllocatedSize();
	}

	for (const TPair<
		FIntPoint,
		TArray<FVoxelRiverSegmentRef>>& Pair :
		RiverSegmentsByHydrologyCell)
	{
		Bytes +=
			Pair.Value.GetAllocatedSize();
	}

	return Bytes;
}
