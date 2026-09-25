#include "Voxel/Map/VoxelMapSurfaceResolver.h"

#include "Voxel/Generation/VoxelGenerationPipeline.h"

bool FVoxelMapSurfaceResolver::Resolve(
	const FVoxelGenerationPipeline& InGenerator,
	const double InBlockSize,
	const FVector2D& InMapPosition,
	FVector& OutLocation,
	FString& OutError)
{
	OutLocation = FVector::ZeroVector;
	if (!FMath::IsFinite(InBlockSize) || InBlockSize <= 0.0 ||
		!FMath::IsFinite(InMapPosition.X) ||
		!FMath::IsFinite(InMapPosition.Y))
	{
		OutError = TEXT("Map surface coordinates or voxel scale are invalid");
		return false;
	}
	const double CellX = FMath::FloorToDouble(InMapPosition.X / InBlockSize);
	const double CellY = FMath::FloorToDouble(InMapPosition.Y / InBlockSize);
	if (CellX < MIN_int32 || CellX >= MAX_int32 ||
		CellY < MIN_int32 || CellY >= MAX_int32)
	{
		OutError = TEXT("Map surface coordinates exceed the voxel world");
		return false;
	}
	FVoxelEnvironmentSample Environment;
	if (!InGenerator.SampleEnvironment(
		static_cast<int32>(CellX), static_cast<int32>(CellY),
		Environment, OutError))
	{
		return false;
	}
	const FVoxelColumnSample& Column = Environment.Column;
	int32 TopZ = Column.SurfaceZ;
	if ((Column.bRiver || Column.bLake || Column.bOcean) &&
		Column.SurfaceWaterZ != MIN_int32)
	{
		TopZ = FMath::Max(TopZ, Column.SurfaceWaterZ);
	}
	OutLocation = FVector(
		InMapPosition.X,
		InMapPosition.Y,
		(static_cast<double>(TopZ) + 2.0) * InBlockSize);
	OutError.Reset();
	return true;
}
