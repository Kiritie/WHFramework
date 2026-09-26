#include "Voxel/Generation/VoxelGenerationPipeline.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/Ecology/VoxelEcology.h"
#include "Voxel/Generation/VoxelGenerationQuery.h"

namespace
{
	bool MakeColumnGridBounds(
		const FVoxelGenerationSettings& InSettings,
		const FIntPoint& InOrigin,
		const int32 InWidth,
		const int32 InHeight,
		const int32 InStep,
		FVoxelGenerationBounds& OutBounds,
		int32& OutCount,
		FString& OutError)
	{
		if (InWidth <= 0 || InHeight <= 0 ||
			InWidth > 4096 || InHeight > 4096 || InStep <= 0)
		{
			OutError = TEXT("Voxel column grid parameters are invalid");
			return false;
		}

		const int64 Count = static_cast<int64>(InWidth) * InHeight;
		const int64 LastX = static_cast<int64>(InOrigin.X) +
			static_cast<int64>(InWidth - 1) * InStep;
		const int64 LastY = static_cast<int64>(InOrigin.Y) +
			static_cast<int64>(InHeight - 1) * InStep;
		if (Count > MAX_int32 || LastX >= MAX_int32 || LastY >= MAX_int32)
		{
			OutError = TEXT("Voxel column grid exceeds coordinate range");
			return false;
		}

		OutBounds = {
			FIntVector(InOrigin.X, InOrigin.Y, InSettings.MinZ),
			FIntVector(static_cast<int32>(LastX + 1),
				static_cast<int32>(LastY + 1), InSettings.MaxZ)
		};
		OutCount = static_cast<int32>(Count);
		OutError.Reset();
		return true;
	}
}

FVoxelGenerationPipeline::FVoxelGenerationPipeline(
	TSharedRef<
		const FVoxelGenerationRuntimeConfig,
		ESPMode::ThreadSafe> InConfig,
	TSharedRef<
		FVoxelGenerationPlanCache,
		ESPMode::ThreadSafe> InCache,
	TSharedPtr<const IVoxelGenerationOverlay, ESPMode::ThreadSafe> InOverlay)
	: Config(InConfig)
	, Cache(InCache)
	, Overlay(MoveTemp(InOverlay))
{
}

bool FVoxelGenerationPipeline::GenerateSection(
	const FIntVector& InSectionCoordinate,
	TArray<FVoxelBlockState>& OutBaseBlocks,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_GenerateSection);

	constexpr int32 SectionSide = 16;
	constexpr int32 SectionCellCount =
		SectionSide *
		SectionSide *
		SectionSide;

	const FIntVector WorldMin =
		InSectionCoordinate *
		SectionSide;

	const FVoxelGenerationBounds Bounds {
		WorldMin,
		WorldMin +
			FIntVector(
				SectionSide)
	};

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError) ||
		!Query.Prepare(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	TArray<FVoxelBlockState> Blocks;
	Blocks.SetNumUninitialized(
		SectionCellCount);

	// 一列共享同一份自然采样，每格仍执行完整生成阶段，避免反复争用列缓存锁。
	TArray<uint32> Symbols;
	for (int32 Y = 0; Y < SectionSide; ++Y)
	{
		for (int32 X = 0; X < SectionSide; ++X)
		{
			if (!Query.SampleColumnSymbols(FIntPoint(WorldMin.X + X, WorldMin.Y + Y), WorldMin.Z,
				SectionSide, Symbols, OutError))
			{
				return false;
			}
			for (int32 Z = 0; Z < SectionSide; ++Z)
			{
				if (!Config->ToRuntime(Symbols[Z], Blocks[X + SectionSide * Y + SectionSide * SectionSide * Z]))
				{
					OutError = TEXT("Voxel generation returned an invalid recipe symbol");
					return false;
				}
			}
		}
	}

	if (Overlay && !Overlay->ApplySection(InSectionCoordinate, Blocks,
		OutError, InCancel))
	{
		return false;
	}

	OutBaseBlocks =
		MoveTemp(Blocks);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& OutColumn,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InX == MAX_int32 || InY == MAX_int32)
	{
		OutError = TEXT("Voxel column exceeds coordinate range");
		return false;
	}

	const FVoxelGenerationBounds Bounds {
		FIntVector(
			InX,
			InY,
			Config->Recipe->
				Settings.MinZ),
		FIntVector(
			InX + 1,
			InY + 1,
			Config->Recipe->
				Settings.MaxZ)
	};

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError) ||
		!Query.PrepareColumns(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	return Query.SampleColumn(
		InX,
		InY,
		OutColumn,
		OutError);
}

bool FVoxelGenerationPipeline::EnumerateTrees(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<void(const FIntVector&, int32)> InVisit,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError)) return false;
	bool bSampleFailed = false;
	auto SampleColumn = [&](const FIntVector& Position, FVoxelColumnSample& Column)
	{
		if (!Query.SampleEnvironmentColumn(Position.X, Position.Y, Column, OutError, InCancel))
		{
			bSampleFailed = true;
			return false;
		}
		return true;
	};
	auto SampleBase = [&](const FIntVector& Position, uint32& Symbol)
	{
		FVoxelColumnSample Column;
		if (!SampleColumn(Position, Column)) return false;
		Symbol = Position.Z <= Column.SurfaceZ ? Config->Recipe->Palette.Stone : Config->Recipe->Palette.Air;
		return true;
	};
	const FVoxelEcologyGenerator Ecology(Config->Recipe.ToSharedRef());
	int32 Candidates = 0;
	int32 Accepted = 0;
	Ecology.EnumerateTrees(InBounds, SampleColumn, SampleBase,
		[&](const FIntVector& Anchor, const int32 Height, const FVoxelStableId)
		{
			InVisit(Anchor, Height);
		}, Candidates, Accepted, InCancel);
	if (bSampleFailed) return false;
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleEnvironment(
	const int32 InX,
	const int32 InY,
	FVoxelEnvironmentSample& OutSample,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (InX == MAX_int32 || InY == MAX_int32)
	{
		OutError = TEXT("Voxel environment exceeds coordinate range");
		return false;
	}

	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError))
	{
		return false;
	}
	return Query.SampleEnvironmentColumn(
		InX, InY, OutSample.Column, OutError, InCancel);
}

bool FVoxelGenerationPipeline::SampleEnvironments(
	const FIntPoint& InOrigin,
	const int32 InWidth,
	const int32 InHeight,
	const int32 InStep,
	TArray<FVoxelEnvironmentSample>& OutSamples,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const bool bInUseColumnCache) const
{
	FVoxelGenerationBounds Bounds;
	int32 Count = 0;
	if (!MakeColumnGridBounds(Config->Recipe->Settings, InOrigin,
		InWidth, InHeight, InStep, Bounds, Count, OutError))
	{
		return false;
	}

	FVoxelGenerationQuery Query;
	if (!FVoxelGenerationQuery::Create(Config, Cache, Query, OutError,
			bInUseColumnCache) ||
		!Query.PrepareColumns(Bounds, OutError, InCancel))
	{
		return false;
	}

	TArray<FVoxelEnvironmentSample> Samples;
	Samples.SetNumUninitialized(Count);
	int32 Index = 0;
	for (int32 Y = 0; Y < InHeight; ++Y)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		for (int32 X = 0; X < InWidth; ++X)
		{
			const int32 WorldX = static_cast<int32>(
				static_cast<int64>(InOrigin.X) + static_cast<int64>(X) * InStep);
			const int32 WorldY = static_cast<int32>(
				static_cast<int64>(InOrigin.Y) + static_cast<int64>(Y) * InStep);
			if (!Query.SampleEnvironmentColumn(WorldX, WorldY,
				Samples[Index++].Column, OutError, InCancel))
			{
				return false;
			}
		}
	}

	OutSamples = MoveTemp(Samples);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleColumns(
	const FIntPoint& InOrigin,
	const int32 InWidth,
	const int32 InHeight,
	const int32 InStep,
	TArray<FVoxelColumnSample>& OutColumns,
	FString& OutError,
	const TAtomic<bool>* InCancel,
	const bool bInUseColumnCache) const
{
	FVoxelGenerationBounds Bounds;
	int32 Count = 0;
	if (!MakeColumnGridBounds(Config->Recipe->Settings, InOrigin,
		InWidth, InHeight, InStep, Bounds, Count, OutError))
	{
		return false;
	}

	FVoxelGenerationQuery Query;

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError,
			bInUseColumnCache) ||
		!Query.PrepareColumns(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	TArray<FVoxelColumnSample> Columns;
	Columns.SetNumUninitialized(Count);

	int32 Index = 0;

	for (int32 Y = 0;
		Y < InHeight;
		++Y)
	{
		if (InCancel &&
			InCancel->Load())
		{
			OutError =
				TEXT("Canceled");

			return false;
		}

		for (int32 X = 0;
			X < InWidth;
			++X)
		{
			if (!Query.SampleColumn(
				static_cast<int32>(static_cast<int64>(InOrigin.X) + static_cast<int64>(X) * InStep),
				static_cast<int32>(static_cast<int64>(InOrigin.Y) + static_cast<int64>(Y) * InStep),
				Columns[Index++],
				OutError))
			{
				return false;
			}
		}
	}

	OutColumns =
		MoveTemp(Columns);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::SampleBlock(
	const FIntVector& InPosition,
	FVoxelBlockState& OutState,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	FVoxelGenerationQuery Query;

	const FVoxelGenerationBounds Bounds {
		InPosition,
		InPosition +
			FIntVector(1)
	};

	if (!FVoxelGenerationQuery::Create(
			Config,
			Cache,
			Query,
			OutError) ||
		!Query.Prepare(
			Bounds,
			OutError,
			InCancel))
	{
		return false;
	}

	uint32 RecipeValue = 0;

	if (!Query.SampleSymbol(
			InPosition,
			RecipeValue,
			OutError) ||
		!Config->ToRuntime(
			RecipeValue,
			OutState))
	{
		if (OutError.IsEmpty())
		{
			OutError =
				TEXT("Voxel generation returned an invalid recipe symbol");
		}

		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPipeline::BuildCoarseOverlay(
	const FIntVector& InOrigin,
	const int32 InStep,
	const int32 InGridSide,
	TMap<FIntVector, FVoxelBlockState>& OutCells,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	if (Overlay)
	{
		return Overlay->BuildCoarse(InOrigin, InStep, InGridSide, OutCells, OutError, InCancel);
	}
	OutCells.Reset();
	OutError.Reset();
	return true;
}
