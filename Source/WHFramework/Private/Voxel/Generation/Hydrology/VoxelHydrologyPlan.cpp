#include "Voxel/Generation/Hydrology/VoxelHydrology.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	int32 ResolveRiverInfluenceRadius(
		const FVoxelRiverRoutePoint& InPoint)
	{
		const int32 BankWidth =
			FMath::Max(
				1,
				InPoint.HalfWidth);

		const int32 ShoreWidth =
			FMath::Max(
				BankWidth + 1,
				InPoint.HalfWidth * 3);

		return
			InPoint.HalfWidth +
			BankWidth +
			ShoreWidth;
	}
}

void FVoxelHydrologyPlan::Finalize()
{
	LakeWaterByCellOrigin.Reset();
	RiverSegmentsByHydrologyCell.Reset();

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
				River.Points.Num();
			++PointIndex)
		{
			const FVoxelRiverRoutePoint& A =
				River.Points[
					PointIndex - 1];

			const FVoxelRiverRoutePoint& B =
				River.Points[
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

			if (Ref.PointIndex <= 0 ||
				!River.Points.IsValidIndex(
					Ref.PointIndex))
			{
				continue;
			}

			const FVoxelRiverRoutePoint& A =
				River.Points[
					Ref.PointIndex - 1];

			const FVoxelRiverRoutePoint& B =
				River.Points[
					Ref.PointIndex];

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

			Shape.HalfWidth =
				A.HalfWidth +
				static_cast<int32>(
					static_cast<int64>(
						B.HalfWidth -
						A.HalfWidth) *
					AlphaQ16 /
					65536);

			Shape.Depth =
				A.Depth +
				static_cast<int32>(
					static_cast<int64>(
						B.Depth -
						A.Depth) *
					AlphaQ16 /
					65536);

			Shape.BankWidth =
				FMath::Max(
					1,
					Shape.HalfWidth);

			Shape.ShoreWidth =
				FMath::Max(
					Shape.BankWidth + 1,
					Shape.HalfWidth * 3);

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

				BestSection =
					Section;

				bHasRiver = true;
			}
		}
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
