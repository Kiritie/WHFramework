#include "Voxel/Rendering/VoxelCoverage.h"

bool VoxelCoverage::IsFullyCovered(
	const FVoxelCoverageRect& InTarget,
	const TConstArrayView<FVoxelCoverageRect> InCoverage)
{
	if (!InTarget.IsValid())
	{
		return false;
	}

	TArray<FVoxelCoverageRect> Clipped;
	TArray<int32> XEdges;
	TArray<int32> YEdges;

	XEdges.Add(
		InTarget.Min.X);

	XEdges.Add(
		InTarget.Max.X);

	YEdges.Add(
		InTarget.Min.Y);

	YEdges.Add(
		InTarget.Max.Y);

	for (const FVoxelCoverageRect& Coverage :
		InCoverage)
	{
		if (!Coverage.IsValid() ||
			!Coverage.Intersects(
				InTarget))
		{
			continue;
		}

		FVoxelCoverageRect Rect;

		Rect.Min.X =
			FMath::Max(
				Coverage.Min.X,
				InTarget.Min.X);

		Rect.Min.Y =
			FMath::Max(
				Coverage.Min.Y,
				InTarget.Min.Y);

		Rect.Max.X =
			FMath::Min(
				Coverage.Max.X,
				InTarget.Max.X);

		Rect.Max.Y =
			FMath::Min(
				Coverage.Max.Y,
				InTarget.Max.Y);

		if (!Rect.IsValid())
		{
			continue;
		}

		Clipped.Add(
			Rect);

		XEdges.AddUnique(
			Rect.Min.X);

		XEdges.AddUnique(
			Rect.Max.X);

		YEdges.AddUnique(
			Rect.Min.Y);

		YEdges.AddUnique(
			Rect.Max.Y);
	}

	if (Clipped.IsEmpty())
	{
		return false;
	}

	XEdges.Sort();
	YEdges.Sort();

	for (int32 XIndex = 0;
		XIndex + 1 <
			XEdges.Num();
		++XIndex)
	{
		const int32 X0 =
			XEdges[XIndex];

		const int32 X1 =
			XEdges[
				XIndex + 1];

		if (X0 >= X1)
		{
			continue;
		}

		for (int32 YIndex = 0;
			YIndex + 1 <
				YEdges.Num();
			++YIndex)
		{
			const int32 Y0 =
				YEdges[
					YIndex];

			const int32 Y1 =
				YEdges[
					YIndex + 1];

			if (Y0 >= Y1)
			{
				continue;
			}

			/**
			 * 该 partition cell 完全在同一组 rect 边界内，
			 * 检查任意一个内部整数点即可。
			 */
			const FIntPoint Probe(
				X0,
				Y0);

			bool bCovered =
				false;

			for (const FVoxelCoverageRect& Rect :
				Clipped)
			{
				if (Rect.ContainsCell(
						Probe))
				{
					bCovered =
						true;

					break;
				}
			}

			if (!bCovered)
			{
				return false;
			}
		}
	}

	return true;
}
