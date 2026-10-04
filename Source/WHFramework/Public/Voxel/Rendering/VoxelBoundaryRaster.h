#pragma once

#include "CoreMinimal.h"

struct WHFRAMEWORK_API FVoxelBoundaryCell
{
	uint16 Material = 0;
	bool bFilled = false;
};

struct WHFRAMEWORK_API FVoxelBoundaryRaster
{
	FIntPoint Min = FIntPoint::ZeroValue;
	FIntPoint Max = FIntPoint::ZeroValue;
	int32 StepCells = 1;
	TArray<FVoxelBoundaryCell> Cells;
	bool Validate(FString& OutError) const;
	const FVoxelBoundaryCell& Sample(int32 InU, int32 InV) const;
};

struct WHFRAMEWORK_API FVoxelBoundaryFace
{
	FVector Vertices[4];
	FVector Normal;
	uint16 Material = 0;
	bool bReverseWinding = false;
};

struct WHFRAMEWORK_API FVoxelHeightBoundary
{
	int32 Min = 0;
	int32 Max = 0;
	int32 StepCells = 1;
	TArray<double> Heights;
	TArray<uint16> Materials;
	bool Validate(FString& OutError) const;
};

// 输入必须来自本代实际渲染数据；旧边界面由这些 patch 替代，不能重复叠加。
class WHFRAMEWORK_API FVoxelBoundaryRasterBuilder
{
public:
	static bool BuildVolume(const FVoxelBoundaryRaster& InNegative, const FVoxelBoundaryRaster& InPositive,
		int32 InAxis, double InPlaneCells, int32 InMaximumFaces,
		TArray<FVoxelBoundaryFace>& OutFaces, FString& OutError);
	static bool BuildHeightfield(const FVoxelHeightBoundary& InNegative, const FVoxelHeightBoundary& InPositive,
		int32 InAxis, double InPlaneCells, int32 InMaximumFaces,
		TArray<FVoxelBoundaryFace>& OutFaces, FString& OutError);

private:
	static FVoxelBoundaryFace MakeFace(int32 InAxis, double InPlane, double InMinU, double InMaxU,
		double InMinV, double InMaxV, bool bInPositiveNormal, uint16 InMaterial);
};
