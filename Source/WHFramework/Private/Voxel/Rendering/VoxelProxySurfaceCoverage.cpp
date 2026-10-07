#include "Voxel/Rendering/VoxelProxyBuilder.h"

bool FVoxelVoxelProxyData::AdvanceSurfaceCoverage(const int32 InMaximumRows, bool& bOutComplete,
	FString& OutError, const TAtomic<bool>* InCancel)
{
	bOutComplete = false;
	if (GridSide <= 0 || GridSide > 256 || Geometry.SampleStepCells <= 0 ||
		Cells.Num() != GridSide * GridSide * GridSide || CapturedSurfaceRows < 0 || CapturedSurfaceRows > GridSide || InMaximumRows <= 0)
	{ OutError = TEXT("Invalid proxy surface coverage input"); return false; }
	if (CapturedSurfaceRows == 0)
	{
		// 上游 raw 完成租约已经包含最坏的逐行交错区间；填充前一次 Reserve。
		TopSurfaceCoverage.Reset();
		TopSurfaceCoverage.Reserve(GridSide * ((GridSide + 1) / 2));
	}
	const auto Origin = GetBuildBounds().Min;
	const int32 Step = Geometry.SampleStepCells;
	const int32 EndRow = FMath::Min(GridSide, CapturedSurfaceRows + InMaximumRows);
	for (; CapturedSurfaceRows < EndRow; ++CapturedSurfaceRows)
	{
		if (InCancel && InCancel->Load()) { OutError = TEXT("Canceled"); return false; }
		int32 RunStart = INDEX_NONE;
		for (int32 X = 0; X <= GridSide; ++X)
		{
			const bool bCovered = X < GridSide && VoxelCoverage::HasProxyTopSurfaceInColumn(*this, X, CapturedSurfaceRows);
			if (bCovered && RunStart == INDEX_NONE) RunStart = X;
			else if (!bCovered && RunStart != INDEX_NONE)
			{
				TopSurfaceCoverage.Add({FIntPoint(Origin.X + RunStart * Step, Origin.Y + CapturedSurfaceRows * Step),
					FIntPoint(Origin.X + X * Step, Origin.Y + (CapturedSurfaceRows + 1) * Step)});
				RunStart = INDEX_NONE;
			}
		}
	}
	bOutComplete = CapturedSurfaceRows == GridSide;
	OutError.Reset();
	return true;
}
