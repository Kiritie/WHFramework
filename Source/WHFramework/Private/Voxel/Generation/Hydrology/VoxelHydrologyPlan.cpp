#include "Voxel/Generation/Hydrology/VoxelHydrologyPlan.h"

#include "Voxel/Generation/VoxelGenerationAllocation.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	int32 NextHydrologyPlanCapacity(const int32 InRequired, const int32 InCapacity)
	{
		return int32(FMath::Min<int64>(MAX_int32,
			FMath::Max<int64>(InRequired, FMath::Max<int64>(4, int64(InCapacity) * 2))));
	}

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
			return static_cast<int32>(int64(First) +
				(int64(Second) - First) * Numerator / Denominator);
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

	bool BuildShapeRoute(const TArray<FVoxelRiverRoutePoint>& InRaw,
		const FIntPoint& InCoreMin, const FIntPoint& InCoreMax,
		const int32 InPasses, TArray<FVoxelRiverShapePoint>& OutShape, FString& OutError)
	{
		OutShape.Reset();
		auto InsideCore = [&InCoreMin, &InCoreMax](const FIntPoint& Position)
		{
			return Position.X >= InCoreMin.X && Position.Y >= InCoreMin.Y &&
				Position.X < InCoreMax.X && Position.Y < InCoreMax.Y;
		};
		int64 ShapeCount = InRaw.Num();
		for (int32 Index = 1; Index < InRaw.Num(); ++Index)
		{
			if (InsideCore(InRaw[Index].Position) != InsideCore(InRaw[Index - 1].Position))
			{
				const auto Boundary = InterpolateShapePoint(ToShapePoint(InRaw[Index - 1]),
					ToShapePoint(InRaw[Index]), 1, 2);
				ShapeCount += Boundary.Position != InRaw[Index - 1].Position &&
					Boundary.Position != InRaw[Index].Position ? 1 : 0;
			}
		}
		if (ShapeCount > MAX_int32)
		{
			OutError = TEXT("Oversized: hydrology shape count exceeds native capacity");
			return false;
		}
		if (!VoxelGenerationAllocation::RetainArray(OutShape, int32(ShapeCount), OutError))
		{
			return false;
		}
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
			FVoxelGenerationWorkspace Workspace;
			TArray<FVoxelRiverShapePoint> Next;
			auto EmitPass = [&OutShape](TFunctionRef<void(const FVoxelRiverShapePoint&)> Emit)
			{
				FVoxelRiverShapePoint Last = OutShape[0];
				auto AddPoint = [&Emit, &Last](const FVoxelRiverShapePoint& Point)
				{
					Emit(Last);
					Last = Point;
				};
				for (int32 Index = 0; Index + 1 < OutShape.Num(); ++Index)
				{
					for (const int32 Fraction : {1, 3})
					{
						const auto Point = InterpolateShapePoint(OutShape[Index], OutShape[Index + 1], Fraction, 4);
						if (Point.Position != Last.Position)
						{
							AddPoint(Point);
						}
					}
					if (OutShape[Index + 1].bAnchor && Index + 2 < OutShape.Num())
					{
						if (OutShape[Index + 1].Position == Last.Position)
						{
							Last.bAnchor = true;
						}
						else
						{
							AddPoint(OutShape[Index + 1]);
						}
					}
				}
				if (Last.Position != OutShape.Last().Position)
				{
					AddPoint(OutShape.Last());
				}
				else
				{
					Last.bAnchor = true;
				}
				Emit(Last);
			};
			int64 NextCount = 0;
			EmitPass([&NextCount](const FVoxelRiverShapePoint&)
			{
				++NextCount;
			});
			if (NextCount > MAX_int32)
			{
				OutError = TEXT("Oversized: hydrology smoothing count exceeds native capacity");
				return false;
			}
			if (!Workspace.ReserveArray(Next, int32(NextCount), OutError))
			{
				return false;
			}
			EmitPass([&Next](const FVoxelRiverShapePoint& Point)
			{
				Next.Add(Point);
			});
			VoxelGenerationAllocation::FreeRetainedArray(OutShape);
			if (!VoxelGenerationAllocation::RetainArray(OutShape, Next.Num(), OutError))
			{
				return false;
			}
			OutShape.Append(Next);
		}
		if (OutShape.Num() < 3)
		{
			return true;
		}
		FVoxelGenerationWorkspace Workspace;
		TArray<FVoxelRiverShapePoint> Original;
		if (!Workspace.ReserveArray(Original, OutShape.Num(), OutError))
		{
			return false;
		}
		Original.Append(OutShape);
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
		return true;
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
		const FVector2D A(double(int64(InCurrent.X) - InPrevious.X),
			double(int64(InCurrent.Y) - InPrevious.Y));
		const FVector2D B(double(int64(InNext.X) - InCurrent.X),
			double(int64(InNext.Y) - InCurrent.Y));
		const double Dot = A.X * B.X + A.Y * B.Y;
		const double Cross = FMath::Abs(A.X * B.Y - A.Y * B.X);
		return Dot > 0 && Cross * 1024 <= Dot * InTangentQ10;
	}

	FVoxelRiverMeanderPoint InterpolateMeanderPoint(const FVoxelRiverMeanderPoint& A,
		const FVoxelRiverMeanderPoint& B, const int32 InNumerator, const int32 InDenominator)
	{
		const auto Blend = [InNumerator, InDenominator](const int32 First, const int32 Second)
		{
			return int32(int64(First) + (int64(Second) - First) * InNumerator / InDenominator);
		};
		FVoxelRiverMeanderPoint Result;
		Result.Position = FIntPoint(Blend(A.Position.X, B.Position.X), Blend(A.Position.Y, B.Position.Y));
		Result.WaterZ = Blend(A.WaterZ, B.WaterZ);
		Result.BedHalfWidth = Blend(A.BedHalfWidth, B.BedHalfWidth);
		Result.WaterHalfWidth = Blend(A.WaterHalfWidth, B.WaterHalfWidth);
		Result.BankWidth = Blend(A.BankWidth, B.BankWidth);
		Result.ShoreWidth = Blend(A.ShoreWidth, B.ShoreWidth);
		Result.Depth = Blend(A.Depth, B.Depth);
		Result.Accumulation = InterpolateAccumulation(A.Accumulation, B.Accumulation, uint32(InNumerator), uint32(InDenominator));
		Result.MeanderOffset = Blend(A.MeanderOffset, B.MeanderOffset);
		return Result;
	}

	struct FMeanderSearchNode
	{
		uint64 Cost = MAX_uint64;
		int32 Parent = INDEX_NONE;
		int32 HeapIndex = INDEX_NONE;
		bool bClosed = false;
	};

	bool RefineQuantizedMeanderTurn(TArray<FVoxelRiverMeanderPoint>& OutPoints,
		const int32 InIndex, const int32 InTangentQ10, const int32 InRadius,
		bool& bOutRefined, FString& OutError, const TAtomic<bool>* InCancel)
	{
		bOutRefined = false;
		const int32 FirstIndex = FMath::Max(0, InIndex - 2);
		const int32 LastIndex = FMath::Min(OutPoints.Num() - 1, InIndex + 2);
		const auto Previous = OutPoints[FirstIndex];
		const auto Next = OutPoints[LastIndex];
		if (Previous.Position == Next.Position) return true;
		// 量化修正只搜索原局部河段的有限邻域。状态包含入射方向与下一固定连接，
		// 所有锚点按原顺序经过；首尾切向也必须与邻域外的原河段匹配。
		const FIntPoint Directions[] = {FIntPoint(1, 0), FIntPoint(2, 1), FIntPoint(1, 1), FIntPoint(1, 2),
			FIntPoint(0, 1), FIntPoint(-1, 2), FIntPoint(-1, 1), FIntPoint(-2, 1),
			FIntPoint(-1, 0), FIntPoint(-2, -1), FIntPoint(-1, -1), FIntPoint(-1, -2),
			FIntPoint(0, -1), FIntPoint(1, -2), FIntPoint(1, -1), FIntPoint(2, -1)};
		const uint32 StepCosts[] = {1024, 2290, 1448, 2290, 1024, 2290, 1448, 2290,
			1024, 2290, 1448, 2290, 1024, 2290, 1448, 2290};
		constexpr int32 DirectionCount = UE_ARRAY_COUNT(Directions);
		constexpr int32 StateDirections = DirectionCount + 1;
		FVoxelRiverMeanderPoint Targets[4];
		int32 TargetCount = 0;
		FIntPoint OriginalMin = Previous.Position;
		FIntPoint OriginalMax = Previous.Position;
		for (int32 Index = FirstIndex + 1; Index <= LastIndex; ++Index)
		{
			const auto& Point = OutPoints[Index];
			OriginalMin.X = FMath::Min(OriginalMin.X, Point.Position.X);
			OriginalMin.Y = FMath::Min(OriginalMin.Y, Point.Position.Y);
			OriginalMax.X = FMath::Max(OriginalMax.X, Point.Position.X);
			OriginalMax.Y = FMath::Max(OriginalMax.Y, Point.Position.Y);
			if (Point.bAnchor || Index == LastIndex) Targets[TargetCount++] = Point;
		}
		const int64 MinX = FMath::Max(VoxelGeneration::WorldCellMin, int64(OriginalMin.X) - InRadius);
		const int64 MinY = FMath::Max(VoxelGeneration::WorldCellMin, int64(OriginalMin.Y) - InRadius);
		const int64 MaxX = FMath::Min(VoxelGeneration::WorldCellMaxExclusive - 1, int64(OriginalMax.X) + InRadius);
		const int64 MaxY = FMath::Min(VoxelGeneration::WorldCellMaxExclusive - 1, int64(OriginalMax.Y) + InRadius);
		const uint64 Width = uint64(MaxX - MinX + 1);
		const uint64 Height = uint64(MaxY - MinY + 1);
		if (Width > MAX_int32 || Height > MAX_int32 || Width * Height > uint64(MAX_int32) / (StateDirections * TargetCount))
		{
			OutError = TEXT("Oversized: hydrology quantized turn search exceeds native capacity");
			return false;
		}
		const int32 StateCount = int32(Width * Height * StateDirections * TargetCount);
		FVoxelGenerationWorkspace Workspace;
		TArray<FMeanderSearchNode> Nodes;
		TArray<int32> Heap;
		TArray<FIntPoint> Path;
		if (!Workspace.ReserveArray(Nodes, StateCount, OutError) ||
			!Workspace.ReserveArray(Heap, StateCount, OutError)) return false;
		Nodes.SetNum(StateCount);
		const auto StateIndex = [MinX, MinY, Width, TargetCount](const FIntPoint& Position, const int32 Direction, const int32 Target)
		{
			return int32(((uint64(int64(Position.Y) - MinY) * Width + uint64(int64(Position.X) - MinX)) *
				TargetCount + Target) * StateDirections + Direction);
		};
		const auto PositionAt = [MinX, MinY, Width, TargetCount](const int32 Index)
		{
			const uint64 Cell = uint64(Index / StateDirections / TargetCount);
			return FIntPoint(int32(MinX + int64(Cell % Width)), int32(MinY + int64(Cell / Width)));
		};
		const auto Less = [&Nodes](const int32 A, const int32 B)
		{
			return Nodes[A].Cost != Nodes[B].Cost ? Nodes[A].Cost < Nodes[B].Cost : A < B;
		};
		const auto SwapHeap = [&Nodes, &Heap](const int32 A, const int32 B)
		{
			Heap.Swap(A, B);
			Nodes[Heap[A]].HeapIndex = A;
			Nodes[Heap[B]].HeapIndex = B;
		};
		const auto PushOrDecrease = [&Nodes, &Heap, &Less, &SwapHeap](const int32 Index)
		{
			int32 Position = Nodes[Index].HeapIndex;
			if (Position == INDEX_NONE)
			{
				Position = Heap.Add(Index);
				Nodes[Index].HeapIndex = Position;
			}
			while (Position > 0)
			{
				const int32 Parent = (Position - 1) / 2;
				if (!Less(Heap[Position], Heap[Parent])) break;
				SwapHeap(Position, Parent);
				Position = Parent;
			}
		};
		const auto Pop = [&Nodes, &Heap, &Less, &SwapHeap]()
		{
			const int32 Result = Heap[0];
			const int32 Last = Heap.Pop(EAllowShrinking::No);
			Nodes[Result].HeapIndex = INDEX_NONE;
			if (!Heap.IsEmpty())
			{
				Heap[0] = Last;
				Nodes[Last].HeapIndex = 0;
				int32 Position = 0;
				while (int64(Position) * 2 + 1 < Heap.Num())
				{
					int32 Child = Position * 2 + 1;
					if (Child + 1 < Heap.Num() && Less(Heap[Child + 1], Heap[Child])) ++Child;
					if (!Less(Heap[Child], Heap[Position])) break;
					SwapHeap(Position, Child);
					Position = Child;
				}
			}
			return Result;
		};
		const auto SegmentDistanceSquared = [](const FIntPoint& Position, const FIntPoint& A, const FIntPoint& B)
		{
			const FVector2D AP(double(int64(Position.X) - A.X), double(int64(Position.Y) - A.Y));
			const FVector2D AB(double(int64(B.X) - A.X), double(int64(B.Y) - A.Y));
			const double Fraction = AB.SizeSquared() > 0 ? FMath::Clamp(FVector2D::DotProduct(AP, AB) / AB.SizeSquared(), 0.0, 1.0) : 0.0;
			return (AP - AB * Fraction).SizeSquared();
		};
		const int32 Initial = StateIndex(Previous.Position, DirectionCount, 0);
		Nodes[Initial].Cost = 0;
		PushOrDecrease(Initial);
		int32 Goal = INDEX_NONE;
		int32 Visited = 0;
		while (!Heap.IsEmpty())
		{
			if ((Visited++ & 255) == 0 && InCancel && InCancel->Load())
			{
				OutError = TEXT("Canceled");
				return false;
			}
			const int32 Index = Pop();
			Nodes[Index].bClosed = true;
			const auto Position = PositionAt(Index);
			const int32 Incoming = Index % StateDirections;
			const int32 Target = Index / StateDirections % TargetCount;
			if (Position == Next.Position)
			{
				if (Target == TargetCount - 1 && Incoming < DirectionCount &&
					(LastIndex + 1 >= OutPoints.Num() || IsMeanderAngleWithinLimit(
						Position - Directions[Incoming], Position, OutPoints[LastIndex + 1].Position, InTangentQ10)))
				{
					Goal = Index;
					break;
				}
				continue;
			}
			for (int32 Direction = 0; Direction < DirectionCount; ++Direction)
			{
				FIntPoint Candidate;
				if (!VoxelGeneration::TryWorldPoint(FInt64Vector2(int64(Position.X) + Directions[Direction].X,
					int64(Position.Y) + Directions[Direction].Y), Candidate) || Candidate.X < MinX || Candidate.X > MaxX ||
					Candidate.Y < MinY || Candidate.Y > MaxY || Candidate == Previous.Position) continue;
				if (Incoming < DirectionCount)
				{
					if (!IsMeanderAngleWithinLimit(FIntPoint::ZeroValue, Directions[Incoming],
						Directions[Incoming] + Directions[Direction], InTangentQ10)) continue;
				}
				else if (FirstIndex > 0 && !IsMeanderAngleWithinLimit(OutPoints[FirstIndex - 1].Position,
					Previous.Position, Candidate, InTangentQ10)) continue;
				bool bWrongTarget = false;
				for (int32 TargetIndex = 0; TargetIndex < TargetCount; ++TargetIndex)
					bWrongTarget |= Candidate == Targets[TargetIndex].Position && TargetIndex != Target;
				if (bWrongTarget) continue;
				const int32 NextTarget = Candidate == Targets[Target].Position && Target + 1 < TargetCount ? Target + 1 : Target;
				const int32 Child = StateIndex(Candidate, Direction, NextTarget);
				if (Nodes[Child].bClosed) continue;
				double DistanceSquared = MAX_dbl;
				for (int32 PointIndex = FirstIndex; PointIndex < LastIndex; ++PointIndex)
					DistanceSquared = FMath::Min(DistanceSquared, SegmentDistanceSquared(Candidate,
						OutPoints[PointIndex].Position, OutPoints[PointIndex + 1].Position));
				const uint64 Cost = Nodes[Index].Cost + StepCosts[Direction] +
					uint64(FMath::RoundToInt(FMath::Min(65535.0, DistanceSquared) * 16.0));
				if (Cost >= Nodes[Child].Cost) continue;
				Nodes[Child].Cost = Cost;
				Nodes[Child].Parent = Index;
				PushOrDecrease(Child);
			}
		}
		if (Goal == INDEX_NONE) return true;
		int32 PathCount = 0;
		for (int32 Index = Goal; Index != INDEX_NONE; Index = Nodes[Index].Parent) ++PathCount;
		if (!Workspace.ReserveArray(Path, PathCount, OutError)) return false;
		Path.SetNum(PathCount);
		for (int32 Index = Goal, Ordinal = PathCount - 1; Index != INDEX_NONE; Index = Nodes[Index].Parent, --Ordinal)
			Path[Ordinal] = PositionAt(Index);
		// 共线细步合并，固定锚点仍是独立正文；合并不改变任何转角。
		for (int32 Index = Path.Num() - 2; Index > 0; --Index)
		{
			bool bAnchor = false;
			for (int32 TargetIndex = 0; TargetIndex < TargetCount; ++TargetIndex)
				bAnchor |= Targets[TargetIndex].bAnchor && Path[Index] == Targets[TargetIndex].Position;
			if (bAnchor) continue;
			const FIntPoint A = Path[Index] - Path[Index - 1];
			const FIntPoint B = Path[Index + 1] - Path[Index];
			if (int64(A.X) * B.Y == int64(A.Y) * B.X) Path.RemoveAt(Index, 1, EAllowShrinking::No);
		}
		Workspace.FreeArray(Heap);
		Workspace.FreeArray(Nodes);
		const int32 RemovedPoints = LastIndex - FirstIndex - 1;
		const int64 Required = int64(OutPoints.Num()) + Path.Num() - 2 - RemovedPoints;
		if (Required > MAX_int32)
		{
			OutError = TEXT("Oversized: hydrology refined route exceeds native capacity");
			return false;
		}
		const int32 Capacity = Required <= OutPoints.Max() ? OutPoints.Max() : NextHydrologyPlanCapacity(int32(Required), OutPoints.Max());
		if (!VoxelGenerationAllocation::RetainArray(OutPoints, Capacity, OutError)) return false;
		OutPoints.RemoveAt(FirstIndex + 1, RemovedPoints, EAllowShrinking::No);
		int32 Target = 0;
		int32 SegmentStart = 0;
		int32 SegmentEnd = Path.IndexOfByKey(Targets[0].Position);
		auto SegmentSource = Previous;
		for (int32 Index = 1; Index + 1 < Path.Num(); ++Index)
		{
			auto Point = Index == SegmentEnd ? Targets[Target] :
				InterpolateMeanderPoint(SegmentSource, Targets[Target], Index - SegmentStart, SegmentEnd - SegmentStart);
			Point.Position = Path[Index];
			OutPoints.Insert(Point, FirstIndex + Index);
			if (Index == SegmentEnd)
			{
				SegmentSource = Targets[Target++];
				SegmentStart = SegmentEnd;
				SegmentEnd = Path.IndexOfByKey(Targets[Target].Position);
			}
		}
		bOutRefined = true;
		return true;
	}

	bool BuildMeanderRoute(const TArray<FVoxelRiverShapePoint>& InShape,
		const int32 InSeed, const int32 InStrength, const int32 InFrequency,
		const int32 InOctaves, const int32 InMaxAngle, const int32 InMinimumStep,
		TArray<FVoxelRiverMeanderPoint>& OutPoints, FString& OutError, const TAtomic<bool>* InCancel)
	{
		OutPoints.Reset();
		if (!VoxelGenerationAllocation::RetainArray(OutPoints, InShape.Num(), OutError))
		{
			return false;
		}
		if (InShape.Num() < 2)
		{
			return true;
		}
		FVoxelGenerationWorkspace Workspace;
		TArray<int32> DistanceToAnchor;
		if (!Workspace.ReserveArray(DistanceToAnchor, InShape.Num(), OutError))
		{
			return false;
		}
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
			const FInt64Vector2 Tangent(
				int64(InShape[Index + 1].Position.X) - InShape[Index - 1].Position.X,
				int64(InShape[Index + 1].Position.Y) - InShape[Index - 1].Position.Y);
			const int64 Length = FMath::Abs(Tangent.X) + FMath::Abs(Tangent.Y);
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
			FIntPoint MeanderPosition;
			if (VoxelGeneration::TryWorldPoint(FInt64Vector2(
				int64(Source.Position.X) - Tangent.Y * Offset / Length,
				int64(Source.Position.Y) + Tangent.X * Offset / Length), MeanderPosition))
			{
				Result.Position = MeanderPosition;
				Result.MeanderOffset = Offset;
			}
		}
		// 细分产生的单格阶梯无法表达平滑转角；按水文网格尺度重采样，固定锚点不移动。
		const int64 MinimumDistanceSquared = static_cast<int64>(InMinimumStep) * InMinimumStep;
		TArray<FVoxelRiverMeanderPoint> Resampled;
		if (!Workspace.ReserveArray(Resampled, OutPoints.Num(), OutError))
		{
			return false;
		}
		auto DistanceSquared = [](const FIntPoint& A, const FIntPoint& B)
		{
			const double X = double(int64(A.X) - B.X);
			const double Y = double(int64(A.Y) - B.Y);
			return X * X + Y * Y;
		};
		for (const FVoxelRiverMeanderPoint& Point : OutPoints)
		{
			if (Point.bAnchor)
			{
				while (Resampled.Num() > 1 && !Resampled.Last().bAnchor &&
					DistanceSquared(Resampled.Last().Position, Point.Position) < MinimumDistanceSquared)
				{
					Resampled.Pop(EAllowShrinking::No);
				}
			}
			if (Resampled.IsEmpty() || Point.bAnchor ||
				DistanceSquared(Resampled.Last().Position, Point.Position) >= MinimumDistanceSquared)
			{
				Resampled.Add(Point);
			}
		}
		VoxelGenerationAllocation::FreeRetainedArray(OutPoints);
		if (!VoxelGenerationAllocation::RetainArray(OutPoints, Resampled.Num(), OutError))
		{
			return false;
		}
		OutPoints.Append(Resampled);
		Workspace.FreeArray(Resampled);
		Workspace.FreeArray(DistanceToAnchor);
		const int32 TangentQ10 = MeanderTangentQ10(InMaxAngle);
		auto PreservesAnchorTurns = [&OutPoints, TangentQ10](
			const int32 Index, const FIntPoint& Position, const bool bRemove)
		{
			if (Index > 1 && OutPoints[Index - 1].bAnchor &&
				!IsMeanderAngleWithinLimit(OutPoints[Index - 2].Position,
					OutPoints[Index - 1].Position,
					bRemove ? OutPoints[Index + 1].Position : Position, TangentQ10))
			{
				return false;
			}
			if (Index + 2 < OutPoints.Num() && OutPoints[Index + 1].bAnchor &&
				!IsMeanderAngleWithinLimit(
					bRemove ? OutPoints[Index - 1].Position : Position,
					OutPoints[Index + 1].Position, OutPoints[Index + 2].Position, TangentQ10))
			{
				return false;
			}
			return true;
		};
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
				// 整步中点可能越过邻接锚点的角度界限；在同一平滑方向上尝试较小步长。
				// 保持锚点和原角度约束，避免量化短线段被卡在无法修正的折角。
				for (int32 Fraction = 4; Fraction > 0; --Fraction)
				{
					const FIntPoint Current = OutPoints[Index].Position;
					const FIntPoint Candidate(
						int32(int64(Current.X) + (int64(Midpoint.X) - Current.X) * Fraction / 4),
						int32(int64(Current.Y) + (int64(Midpoint.Y) - Current.Y) * Fraction / 4));
					if (Candidate == Current || Candidate == OutPoints[Index - 1].Position ||
						Candidate == OutPoints[Index + 1].Position || !PreservesAnchorTurns(Index, Candidate, false)) continue;
					OutPoints[Index].Position = Candidate;
					bChanged = true;
					break;
				}
			}
			if (!bChanged) break;
		}
		for (int32 Index = OutPoints.Num() - 2; Index > 0; --Index)
		{
			if (OutPoints[Index].bAnchor) continue;
			if ((OutPoints[Index].Position == OutPoints[Index - 1].Position ||
				OutPoints[Index].Position == OutPoints[Index + 1].Position) &&
				PreservesAnchorTurns(Index, OutPoints[Index].Position, true))
			{
				OutPoints.RemoveAt(Index, 1, EAllowShrinking::No);
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
			int32 First = Index;
			int32 Last = Index;
			while (First > 1 && !OutPoints[First - 1].bAnchor) --First;
			while (Last + 2 < OutPoints.Num() && !OutPoints[Last + 1].bAnchor) ++Last;
			bool bRemoved = false;
			for (int32 Count = 1; Count <= Last - First + 1 && !bRemoved; ++Count)
			{
				for (int32 Begin = FMath::Max(First, Index - Count + 1);
					Begin <= Index && Begin + Count - 1 <= Last; ++Begin)
				{
					const int32 Before = Begin - 1;
					const int32 After = Begin + Count;
					if (OutPoints[Before].Position == OutPoints[After].Position ||
						int64(OutPoints[After].WaterZ) > int64(OutPoints[Before].WaterZ) + 1) continue;
					if (Before > 0 && !IsMeanderAngleWithinLimit(
						OutPoints[Before - 1].Position, OutPoints[Before].Position,
						OutPoints[After].Position, TangentQ10)) continue;
					if (After + 1 < OutPoints.Num() && !IsMeanderAngleWithinLimit(
						OutPoints[Before].Position, OutPoints[After].Position,
						OutPoints[After + 1].Position, TangentQ10)) continue;
					// 同时简化量化折角两侧，保留固定锚点以及两端的角度和水位约束。
					OutPoints.RemoveAt(Begin, Count, EAllowShrinking::No);
					Index = FMath::Max(1, Begin - 1);
					bRemoved = true;
					break;
				}
			}
			if (!bRemoved) ++Index;
		}
		// 对最后仍超限的量化转角使用同一组局部格点、角度和固定连接约束。
		for (int32 Index = 1; Index + 1 < OutPoints.Num(); ++Index)
		{
			if (IsMeanderAngleWithinLimit(OutPoints[Index - 1].Position,
				OutPoints[Index].Position, OutPoints[Index + 1].Position, TangentQ10)) continue;
			const int32 BeforeCount = OutPoints.Num();
			bool bRefined = false;
			if (!RefineQuantizedMeanderTurn(OutPoints, Index, TangentQ10, InMinimumStep,
				bRefined, OutError, InCancel)) return false;
			if (bRefined) Index += OutPoints.Num() - BeforeCount;
		}
		for (int32 Index = 1; Index + 1 < OutPoints.Num(); ++Index)
		{
			const FVector2D A(double(int64(OutPoints[Index].Position.X) - OutPoints[Index - 1].Position.X),
				double(int64(OutPoints[Index].Position.Y) - OutPoints[Index - 1].Position.Y));
			const FVector2D B(double(int64(OutPoints[Index + 1].Position.X) - OutPoints[Index].Position.X),
				double(int64(OutPoints[Index + 1].Position.Y) - OutPoints[Index].Position.Y));
			const double Cross = A.X * B.Y - A.Y * B.X;
			OutPoints[Index].Curvature = Cross > 0 ? 1 : Cross < 0 ? -1 : 0;
		}
		return true;
	}

	int64 ResolveRiverInfluenceRadius(
		const FVoxelRiverMeanderPoint& InPoint)
	{
		return int64(InPoint.WaterHalfWidth) + InPoint.BankWidth +
			InPoint.ShoreWidth;
	}
}

bool FVoxelHydrologyPlan::Finalize(FString& OutError, const TAtomic<bool>* InCancel)
{
	const auto* Allocation = FVoxelGenerationAllocationScope::Get();
	if (!(Allocation ? Allocation->Check(OutError) : FVoxelGenerationAllocationScope::AllowPassive(OutError)))
	{
		return false;
	}
	LakeWaterByCellOrigin.Reset();
	for (auto& Pair : RiverSegmentsByHydrologyCell)
	{
		VoxelGenerationAllocation::FreeRetainedArray(Pair.Value);
	}
	RiverSegmentsByHydrologyCell.Reset();
	Waterfalls.Reset();
	for (FVoxelRiverRoute& River : Rivers)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		if (!BuildShapeRoute(River.Points, CoreMin, CoreMax,
			RiverShapeSmoothingPasses,
			River.ShapePoints, OutError) ||
			!BuildMeanderRoute(River.ShapePoints, RiverSeed,
			RiverMeanderStrength, RiverMeanderFrequency,
			RiverMeanderOctaves, RiverMaxMeanderAngle,
			FMath::Clamp(Grid.CellSize / 2, 2, 4),
			River.MeanderPoints, OutError, InCancel))
		{
			return false;
		}
		// Author steep drops once in the immutable hydrology plan; clients only present these records.
		for (int32 Start=0; Start+1<River.MeanderPoints.Num(); ++Start)
		{
			const auto& Up = River.MeanderPoints[Start];
			if (Up.Position.X<CoreMin.X || Up.Position.Y<CoreMin.Y || Up.Position.X>=CoreMax.X || Up.Position.Y>=CoreMax.Y) continue;
			for (int32 End=Start+1; End<River.MeanderPoints.Num(); ++End)
			{
				const auto& Down=River.MeanderPoints[End];
				const double Run=FVector2D(double(int64(Down.Position.X)-Up.Position.X),
					double(int64(Down.Position.Y)-Up.Position.Y)).Size();
				if (Run>24.0) break;
				const int64 Drop=int64(Up.WaterZ)-Down.WaterZ;
				if (Drop<8 || double(Drop)*2.0<Run) continue;
				FVoxelWaterfallSemantic Fall;
				Fall.Upstream=FIntVector(Up.Position.X,Up.Position.Y,Up.WaterZ);
				Fall.Downstream=FIntVector(Down.Position.X,Down.Position.Y,Down.WaterZ);
				Fall.HalfWidthCells=FMath::Min(Up.WaterHalfWidth,Down.WaterHalfWidth);
				Fall.Id.High=VoxelGeneration::Mix(uint64(uint32(Up.Position.X))<<32 | uint32(Up.Position.Y));
				Fall.Id.Low=VoxelGeneration::Mix(uint64(uint32(Down.Position.X))<<32 | uint32(Down.Position.Y)) ^ VoxelGeneration::Mix(uint64(uint32(RiverSeed)));
				if (Waterfalls.Num() == Waterfalls.Max() && !VoxelGenerationAllocation::RetainArray(
					Waterfalls, NextHydrologyPlanCapacity(Waterfalls.Num() + 1, Waterfalls.Max()), OutError))
				{
					return false;
				}
				Waterfalls.Add(Fall);
				Start=End;
				break;
			}
		}
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

	int32 LakeIndexCapacity = 0;
	for (const FVoxelLakePlan& Lake :
		Lakes)
	{
		for (const FIntPoint& Cell :
			Lake.Cells)
		{
			if (!LakeWaterByCellOrigin.Contains(Cell) && LakeWaterByCellOrigin.Num() == LakeIndexCapacity)
			{
				const int32 Capacity = NextHydrologyPlanCapacity(LakeIndexCapacity + 1, LakeIndexCapacity);
				if (!VoxelGenerationAllocation::RetainMap(LakeWaterByCellOrigin, Capacity, OutError))
				{
					return false;
				}
				LakeIndexCapacity = Capacity;
			}
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
		OutError.Reset();
		return true;
	}

	auto VisitSegments = [this, InCancel, &OutError](TFunctionRef<bool(const FIntPoint&, const FVoxelRiverSegmentRef&)> Visit)
	{
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
				if (InCancel && InCancel->Load())
				{
					OutError = TEXT("Canceled");
					return false;
				}
				const FVoxelRiverMeanderPoint& A =
					River.MeanderPoints[
						PointIndex - 1];

				const FVoxelRiverMeanderPoint& B =
					River.MeanderPoints[
						PointIndex];

				const int64 Radius =
					FMath::Max(
						ResolveRiverInfluenceRadius(A),
						ResolveRiverInfluenceRadius(B));

				const int64 MinX = FMath::Max(int64(FMath::Min(A.Position.X, B.Position.X)) - Radius,
					VoxelGeneration::WorldCellMin);
				const int64 MinY = FMath::Max(int64(FMath::Min(A.Position.Y, B.Position.Y)) - Radius,
					VoxelGeneration::WorldCellMin);
				const int64 MaxX = FMath::Min(int64(FMath::Max(A.Position.X, B.Position.X)) + Radius,
					VoxelGeneration::WorldCellMaxExclusive - 1);
				const int64 MaxY = FMath::Min(int64(FMath::Max(A.Position.Y, B.Position.Y)) + Radius,
					VoxelGeneration::WorldCellMaxExclusive - 1);
				// 索引只覆盖本 plan 的物理 Grid；Sample 不消费 Grid 外的水文 cell。
				const int64 MinCellX = FMath::Max(VoxelGeneration::FloorDivideWide(MinX, Grid.CellSize), int64(Grid.WorldMinCell.X));
				const int64 MinCellY = FMath::Max(VoxelGeneration::FloorDivideWide(MinY, Grid.CellSize), int64(Grid.WorldMinCell.Y));
				const int64 MaxCellX = FMath::Min(VoxelGeneration::FloorDivideWide(MaxX, Grid.CellSize), int64(Grid.WorldMinCell.X) + Grid.Width - 1);
				const int64 MaxCellY = FMath::Min(VoxelGeneration::FloorDivideWide(MaxY, Grid.CellSize), int64(Grid.WorldMinCell.Y) + Grid.Height - 1);

				for (int64 CellY = MinCellY;
					CellY <= MaxCellY;
					++CellY)
				{
					if (InCancel && InCancel->Load())
					{
						OutError = TEXT("Canceled");
						return false;
					}
					for (int64 CellX = MinCellX;
						CellX <= MaxCellX;
						++CellX)
					{
						if (!Visit(FIntPoint(int32(CellX), int32(CellY)), {RiverIndex, PointIndex}))
						{
							return false;
						}
					}
				}
			}
		}
		return true;
	};
	FVoxelGenerationWorkspace Workspace;
	TMap<FIntPoint, int32> SegmentCounts;
	int32 SegmentCountCapacity = 0;
	if (!VisitSegments([&](const FIntPoint& Cell, const FVoxelRiverSegmentRef&)
	{
		if (int32* Count = SegmentCounts.Find(Cell))
		{
			if (*Count == MAX_int32)
			{
				OutError = TEXT("Oversized: hydrology segment group exceeds native capacity");
				return false;
			}
			++*Count;
		}
		else
		{
			if (SegmentCounts.Num() == SegmentCountCapacity)
			{
				const int32 Capacity = NextHydrologyPlanCapacity(SegmentCountCapacity + 1, SegmentCountCapacity);
				if (!Workspace.ReserveMap(SegmentCounts, Capacity, OutError))
				{
					return false;
				}
				SegmentCountCapacity = Capacity;
			}
			SegmentCounts.Add(Cell, 1);
		}
		return true;
	}))
	{
		return false;
	}
	if (!VoxelGenerationAllocation::RetainMap(RiverSegmentsByHydrologyCell, SegmentCounts.Num(), OutError))
	{
		return false;
	}
	for (const auto& Pair : SegmentCounts)
	{
		auto& Segments = RiverSegmentsByHydrologyCell.FindOrAdd(Pair.Key);
		if (!VoxelGenerationAllocation::RetainArray(Segments, Pair.Value, OutError))
		{
			return false;
		}
	}
	if (!VisitSegments([&](const FIntPoint& Cell, const FVoxelRiverSegmentRef& Ref)
	{
		RiverSegmentsByHydrologyCell.FindChecked(Cell).Add(Ref);
		return true;
	}))
	{
		return false;
	}
	OutError.Reset();
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void FVoxelHydrologyPlan::Finalize()
{
	FString Error;
	const bool bFinalized = Finalize(Error);
	checkf(bFinalized, TEXT("Hydrology fixture finalization failed: %s"), *Error);
}
#endif

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
			if (!VoxelGeneration::IsWorldCell(Point.Position.X) || !VoxelGeneration::IsWorldCell(Point.Position.Y))
			{
				OutError = TEXT("River shape contains a point outside world cell domain");
				return false;
			}
			if (Point.BedHalfWidth < 1 || Point.WaterHalfWidth < Point.BedHalfWidth ||
				Point.BankWidth < 1 || Point.ShoreWidth < 1 || Point.Depth < 1)
			{
				OutError = TEXT("River shape contains an invalid width or depth");
				return false;
			}
			if (Index == 0) continue;
			const FVoxelRiverMeanderPoint& Previous = River.MeanderPoints[Index - 1];
			if (Point.Position == Previous.Position || int64(Point.WaterZ) > int64(Previous.WaterZ) + 1)
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
	if (!VoxelGeneration::IsWorldCell(InWorldX) || !VoxelGeneration::IsWorldCell(InWorldY) ||
		Grid.CellSize <= 0 ||
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

	const FInt64Vector2 LocalWide(int64(WorldHydrologyCell.X) - Grid.WorldMinCell.X,
		int64(WorldHydrologyCell.Y) - Grid.WorldMinCell.Y);

	if (LocalWide.X < 0 || LocalWide.Y < 0 ||
		LocalWide.X >= Grid.Width || LocalWide.Y >= Grid.Height)
	{
		return false;
	}
	const FIntPoint LocalCell(int32(LocalWide.X), int32(LocalWide.Y));

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

	FIntPoint SampleCellOrigin;
	if (!Grid.TryToWorldVoxelXY(GridIndex, SampleCellOrigin)) return false;

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

			const double LengthSquared = double(ABX) * ABX + double(ABY) * ABY;

			const double Alpha = LengthSquared > 0.0 ? FMath::Clamp(
				(double(APX) * ABX + double(APY) * ABY) / LengthSquared, 0.0, 1.0) : 0.0;
			const int32 AlphaQ16 = int32(Alpha * 65536.0);
			// Q16 只插值内容属性；超长段不能用量化 Alpha 求几何最近点。
			const double DX = FMath::Abs(double(APX) - double(ABX) * Alpha);
			const double DY = FMath::Abs(double(APY) - double(ABY) * Alpha);
			const int32 Distance = int32(FMath::Min(double(MAX_int32),
				FMath::Max(DX, DY) + FMath::Min(DX, DY) * 3.0 / 8.0));

			if (Distance >=
				BestDistance)
			{
				continue;
			}

			auto Blend = [AlphaQ16](const int32 First, const int32 Second)
			{
				return int32(int64(First) + (int64(Second) - First) * AlphaQ16 / 65536);
			};
			const int32 WaterZ = Blend(A.WaterZ, B.WaterZ);

			FVoxelRiverShape Shape;

			Shape.BedHalfWidth = Blend(A.BedHalfWidth, B.BedHalfWidth);
			Shape.WaterHalfWidth = Blend(A.WaterHalfWidth, B.WaterHalfWidth);
			Shape.Depth = Blend(A.Depth, B.Depth);
			Shape.BankWidth = Blend(A.BankWidth, B.BankWidth);
			Shape.ShoreWidth = Blend(A.ShoreWidth, B.ShoreWidth);
			const int32 Curvature = AlphaQ16 < 32768 ? A.Curvature : B.Curvature;
			const double Side = double(ABX) * APY - double(ABY) * APX;
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
		VoxelGenerationAllocation::AllocatedBytes(Grid.GroundPlane) +
		VoxelGenerationAllocation::AllocatedBytes(Grid.RainWeight) +
		VoxelGenerationAllocation::AllocatedBytes(Grid.Allowed) +
		VoxelGenerationAllocation::AllocatedBytes(Grid.OceanOutlets) +
		VoxelGenerationAllocation::AllocatedBytes(Grid.CanonicalSinkOutlets) +
		VoxelGenerationAllocation::AllocatedBytes(Drainage.SpillPlane) +
		VoxelGenerationAllocation::AllocatedBytes(Drainage.Parent) +
		VoxelGenerationAllocation::AllocatedBytes(Drainage.SettledOrder) +
		VoxelGenerationAllocation::AllocatedBytes(Drainage.Accumulation) +
		VoxelGenerationAllocation::AllocatedBytes(Rivers) +
		VoxelGenerationAllocation::AllocatedBytes(Lakes) +
		VoxelGenerationAllocation::AllocatedBytes(Waterfalls) +
		VoxelGenerationAllocation::AllocatedBytes(LakeWaterByCellOrigin) +
		VoxelGenerationAllocation::AllocatedBytes(RiverSegmentsByHydrologyCell);

	for (const FVoxelRiverRoute& River :
		Rivers)
	{
		Bytes += VoxelGenerationAllocation::AllocatedBytes(River.Points);
		Bytes += VoxelGenerationAllocation::AllocatedBytes(River.ShapePoints);
		Bytes += VoxelGenerationAllocation::AllocatedBytes(River.MeanderPoints);
	}

	for (const FVoxelLakePlan& Lake :
		Lakes)
	{
		Bytes += VoxelGenerationAllocation::AllocatedBytes(Lake.Cells);
	}

	for (const TPair<
		FIntPoint,
		TArray<FVoxelRiverSegmentRef>>& Pair :
		RiverSegmentsByHydrologyCell)
	{
		Bytes += VoxelGenerationAllocation::AllocatedBytes(Pair.Value);
	}

	return Bytes;
}
