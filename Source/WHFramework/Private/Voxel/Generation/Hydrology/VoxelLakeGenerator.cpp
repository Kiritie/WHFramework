#include "Voxel/Generation/Hydrology/VoxelLakeGenerator.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Voxel/Generation/Hydrology/VoxelRiverGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	double RandomUnit(
		const uint64 InSeed)
	{
		return
			static_cast<double>(
				InSeed &
				0x00ffffffull) /
			static_cast<double>(
				0x01000000ull);
	}

	int32 RandomInt(
		const uint64 InSeed,
		const int32 InMin,
		const int32 InMax)
	{
		return VoxelGeneration::RandomRange(
			InSeed,
			InMin,
			InMax);
	}
}

FVoxelLakeGenerator::FVoxelLakeGenerator(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
	TSharedRef<const FVoxelRiverGenerator, ESPMode::ThreadSafe> InRiver)
	: Recipe(InRecipe)
	, Terrain(InTerrain)
	, River(InRiver)
{
}

bool FVoxelLakeGenerator::ApplyToColumn(
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& InOutColumn) const
{
	if (InOutColumn.bOcean)
	{
		return false;
	}

	const int32 AnchorX =
		VoxelGeneration::FloorDivide(
			InX,
			AnchorSide);

	const int32 AnchorY =
		VoxelGeneration::FloorDivide(
			InY,
			AnchorSide);

	bool bAffected = false;

	for (int32 OffsetY = -1;
		OffsetY <= 1;
		++OffsetY)
	{
		for (int32 OffsetX = -1;
			OffsetX <= 1;
			++OffsetX)
		{
			FVoxelLakeFeature Feature;

			if (!TryGetFeature(
				FIntPoint(
					AnchorX +
						OffsetX,
					AnchorY +
						OffsetY),
				Feature))
			{
				continue;
			}

			int32 BedZ = 0;
			int32 WaterZ = 0;

			if (!EvaluateFeature(
				Feature,
				InX,
				InY,
				InOutColumn.SurfaceZ,
				BedZ,
				WaterZ))
			{
				continue;
			}

			InOutColumn.SurfaceZ =
				FMath::Min(
					InOutColumn.SurfaceZ,
					BedZ);

			InOutColumn.SurfaceWaterZ =
				FMath::Max(
					InOutColumn.SurfaceWaterZ,
					WaterZ);

			InOutColumn.bLake = true;
			InOutColumn.bRiver = false;

			bAffected = true;
		}
	}

	return bAffected;
}

bool FVoxelLakeGenerator::BuildPlan(
	const FVoxelLakeAnchorKey& InKey,
	FVoxelLakeAnchorPlan& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_LakePlan);

	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}

	FVoxelLakeAnchorPlan Plan;
	Plan.Key = InKey;
	Plan.bHasFeature = TryGetFeature(InKey.Coordinate, Plan.Feature);
	OutPlan = MoveTemp(Plan);
	OutError.Reset();
	return true;
}

bool FVoxelLakeGenerator::ApplyFeature(
	const FVoxelLakeAnchorPlan& InPlan,
	const int32 InX,
	const int32 InY,
	FVoxelColumnSample& InOutColumn) const
{
	if (InOutColumn.bOcean || !InPlan.bHasFeature || !InPlan.Feature.IsValid())
	{
		return false;
	}

	int32 BedZ = 0;
	int32 WaterZ = 0;
	if (!EvaluateFeature(InPlan.Feature, InX, InY, InOutColumn.SurfaceZ, BedZ, WaterZ))
	{
		return false;
	}

	InOutColumn.SurfaceZ = FMath::Min(InOutColumn.SurfaceZ, BedZ);
	InOutColumn.SurfaceWaterZ = FMath::Max(InOutColumn.SurfaceWaterZ, WaterZ);
	InOutColumn.bLake = true;
	InOutColumn.bRiver = false;
	return true;
}

bool FVoxelLakeGenerator::TryGetFeature(
	const FIntPoint& InAnchor,
	FVoxelLakeFeature& OutFeature) const
{
	if (const TOptional<FVoxelLakeFeature>* Cached =
		FeatureCache.Find(InAnchor))
	{
		if (!Cached->IsSet())
		{
			return false;
		}

		OutFeature =
			Cached->GetValue();

		return true;
	}

	FVoxelLakeFeature Feature;

	if (!BuildFeature(
		InAnchor,
		Feature))
	{
		FeatureCache.Add(
			InAnchor,
			TOptional<FVoxelLakeFeature>());

		return false;
	}

	FeatureCache.Add(
		InAnchor,
		Feature);

	OutFeature =
		Feature;

	return true;
}

bool FVoxelLakeGenerator::BuildFeature(
	const FIntPoint& InAnchor,
	FVoxelLakeFeature& OutFeature) const
{
	const uint64 AnchorSeed =
		VoxelGeneration::MakeSeed(
			Recipe->Settings.Seed,
			FIntVector(
				InAnchor.X,
				InAnchor.Y,
				0),
			0x4C414B45414E4348ull);

	if (RandomInt(
			AnchorSeed,
			0,
			999) >=
		SpawnPermille)
	{
		return false;
	}

	const FIntPoint AnchorMin(
		InAnchor.X *
			AnchorSide,
		InAnchor.Y *
			AnchorSide);

	FIntPoint BestCenter =
		AnchorMin +
		FIntPoint(
			AnchorSide / 2,
			AnchorSide / 2);

	int32 BestScore =
		MAX_int32;

	int32 BestNormalizedRiverDistance =
		MAX_int32;

	constexpr int32 CandidateGridSide = 5;

	for (int32 GridY = 0;
		GridY < CandidateGridSide;
		++GridY)
	{
		for (int32 GridX = 0;
			GridX < CandidateGridSide;
			++GridX)
		{
			const uint64 CandidateSeed =
				VoxelGeneration::Mix(
					AnchorSeed ^
					static_cast<uint64>(
						GridX +
						GridY *
							CandidateGridSide +
						1));

			const int32 JitterX =
				RandomInt(
					CandidateSeed,
					0,
					FMath::Max(
						0,
						AnchorSide /
							CandidateGridSide -
							1));

			const int32 JitterY =
				RandomInt(
					VoxelGeneration::Mix(
						CandidateSeed),
					0,
					FMath::Max(
						0,
						AnchorSide /
							CandidateGridSide -
							1));

			const FIntPoint Candidate(
				AnchorMin.X +
					GridX *
						AnchorSide /
						CandidateGridSide +
					JitterX,
				AnchorMin.Y +
					GridY *
						AnchorSide /
						CandidateGridSide +
					JitterY);

			const FVoxelMacroTerrainSample TerrainSample =
				Terrain->SampleMacro(
					Candidate.X,
					Candidate.Y);

			if (TerrainSample.SurfaceZ <=
				Recipe->Settings.SeaLevel)
			{
				continue;
			}

			const int32 NormalizedRiverDistance =
				River->
					SampleNormalizedDistanceQ16(
						Candidate.X,
						Candidate.Y);

			const int32 Score =
				TerrainSample.SurfaceZ *
					65536 +
				NormalizedRiverDistance *
					8;

			if (Score <
				BestScore)
			{
				BestCenter =
					Candidate;

				BestScore =
					Score;

				BestNormalizedRiverDistance =
					NormalizedRiverDistance;
			}
		}
	}

	if (BestNormalizedRiverDistance >
		2 *
			65536)
	{
		return false;
	}

	static const FIntPoint Directions[] =
	{
		FIntPoint(1, 0),
		FIntPoint(-1, 0),
		FIntPoint(0, 1),
		FIntPoint(0, -1),
		FIntPoint(1, 1),
		FIntPoint(-1, 1),
		FIntPoint(1, -1),
		FIntPoint(-1, -1)
	};

	static constexpr int32 SearchSteps[] =
	{
		8,
		4,
		2,
		1
	};

	for (const int32 Step :
		SearchSteps)
	{
		for (int32 Iteration = 0;
			Iteration < 4;
			++Iteration)
		{
			FIntPoint Next =
				BestCenter;

			const FVoxelMacroTerrainSample CurrentTerrain =
				Terrain->SampleMacro(
					BestCenter.X,
					BestCenter.Y);

			int32 NextScore =
				CurrentTerrain.SurfaceZ *
					65536 +
				River->
					SampleNormalizedDistanceQ16(
						BestCenter.X,
						BestCenter.Y) *
					8;

			for (const FIntPoint& Direction :
				Directions)
			{
				const FIntPoint Candidate =
					BestCenter +
					Direction *
						Step;

				const FVoxelMacroTerrainSample CandidateTerrain =
					Terrain->SampleMacro(
						Candidate.X,
						Candidate.Y);

				if (CandidateTerrain.SurfaceZ <=
					Recipe->Settings.SeaLevel)
				{
					continue;
				}

				const int32 Score =
					CandidateTerrain.SurfaceZ *
						65536 +
					River->
						SampleNormalizedDistanceQ16(
							Candidate.X,
							Candidate.Y) *
						8;

				if (Score <
					NextScore)
				{
					Next =
						Candidate;

					NextScore =
						Score;
				}
			}

			if (Next ==
				BestCenter)
			{
				break;
			}

			BestCenter =
				Next;
		}
	}

	if (River->
		SampleNormalizedDistanceQ16(
			BestCenter.X,
			BestCenter.Y) >
		75264)
	{
		return false;
	}

	const uint64 FeatureSeed =
		VoxelGeneration::MakeSeed(
			Recipe->Settings.Seed,
			FIntVector(
				BestCenter.X,
				BestCenter.Y,
				0),
			0x4C414B4546454154ull);

	const int32 MajorRadius =
		RandomInt(
			FeatureSeed,
			MinimumRadius * 2,
			MaximumRadius);

	const int32 AspectPermille =
		RandomInt(
			VoxelGeneration::Mix(
				FeatureSeed),
			1700,
			3800);

	const int32 MinorRadius =
		FMath::Clamp(
			MajorRadius *
				1000 /
				FMath::Max(
					1000,
					AspectPermille),
			MinimumRadius,
			FMath::Max(
				MinimumRadius,
				MajorRadius *
					72 /
					100));

	const int32 Depth =
		RandomInt(
			VoxelGeneration::Mix(
				FeatureSeed ^
				0x913AC4ull),
			MinimumDepth,
			MaximumDepth);

	const FVector2D RiverDirection =
		River->SampleRiverDirection(
			BestCenter.X,
			BestCenter.Y);

	const double DirectionJitter =
		(
			RandomUnit(
				VoxelGeneration::Mix(
					FeatureSeed ^
					0x10F19Aull)) *
				2.0 -
			1.0
		) *
		0.35;

	FVoxelLakeFeature Feature;

	Feature.Center =
		BestCenter;

	Feature.WaterZ =
		River->
			SampleWaterHeight();

	Feature.MajorRadius =
		MajorRadius;

	Feature.MinorRadius =
		MinorRadius;

	Feature.Depth =
		Depth;

	Feature.Rotation =
		FMath::Atan2(
			RiverDirection.Y,
			RiverDirection.X) +
		DirectionJitter;

	if (!ValidateBoundary(
		Feature))
	{
		return false;
	}

	OutFeature =
		Feature;

	return true;
}

bool FVoxelLakeGenerator::EvaluateFeature(
	const FVoxelLakeFeature& InFeature,
	const int32 InX,
	const int32 InY,
	const int32 InOriginalHeight,
	int32& OutBedZ,
	int32& OutWaterZ) const
{
	const double Alpha =
		CalculateShapeAlpha(
			InFeature,
			InX,
			InY);

	if (Alpha >= 1.0)
	{
		return false;
	}

	if (InOriginalHeight <=
		Recipe->Settings.SeaLevel)
	{
		return false;
	}

	const double HeightAboveWater =
		FMath::Max(
			static_cast<double>(
				InOriginalHeight -
				InFeature.WaterZ),
			0.0);

	const double AdjustedAlpha =
		Alpha +
		HeightAboveWater *
			0.16;

	if (AdjustedAlpha >= 1.0)
	{
		return false;
	}

	const double SmoothAlpha =
		AdjustedAlpha *
		AdjustedAlpha *
		(
			3.0 -
			2.0 *
				AdjustedAlpha
		);

	const double Depression =
		FMath::Lerp(
			static_cast<double>(
				InFeature.Depth),
			0.75,
			SmoothAlpha);

	OutWaterZ =
		InFeature.WaterZ;

	OutBedZ =
		FMath::FloorToInt(
			InFeature.WaterZ -
			Depression);

	return true;
}

double FVoxelLakeGenerator::CalculateShapeAlpha(
	const FVoxelLakeFeature& InFeature,
	const int32 InX,
	const int32 InY) const
{
	const double DeltaX =
		static_cast<double>(
			InX -
			InFeature.Center.X);

	const double DeltaY =
		static_cast<double>(
			InY -
			InFeature.Center.Y);

	const double CosAngle =
		FMath::Cos(
			InFeature.Rotation);

	const double SinAngle =
		FMath::Sin(
			InFeature.Rotation);

	const double LocalX =
		DeltaX *
			CosAngle +
		DeltaY *
			SinAngle;

	const double LocalY =
		-DeltaX *
			SinAngle +
		DeltaY *
			CosAngle;

	const double EllipseDistance =
		FMath::Sqrt(
			FMath::Square(
				LocalX /
				FMath::Max(
					1,
					InFeature.
						MajorRadius)) +
			FMath::Square(
				LocalY /
				FMath::Max(
					1,
					InFeature.
						MinorRadius)));

	const int32 ShoreNoise =
		VoxelGeneration::Noise2D(
			Recipe->Settings.Seed,
			InX,
			InY,
			67,
			0x11695DF673199E07ull);

	const double ShoreVariation =
		1.0 +
		static_cast<double>(
			ShoreNoise) /
			32768.0 *
			0.18;

	return
		EllipseDistance /
		FMath::Max(
			0.7,
			ShoreVariation);
}

bool FVoxelLakeGenerator::ValidateBoundary(
	const FVoxelLakeFeature& InFeature) const
{
	const int32 BoundaryRadius =
		FMath::CeilToInt(
			InFeature.MajorRadius *
				1.25) +
		2;

	constexpr int32 SampleCount = 24;

	for (int32 Index = 0;
		Index < SampleCount;
		++Index)
	{
		const double Angle =
			2.0 *
			PI *
			Index /
			SampleCount;

		const int32 X =
			InFeature.Center.X +
			FMath::RoundToInt(
				FMath::Cos(Angle) *
					BoundaryRadius);

		const int32 Y =
			InFeature.Center.Y +
			FMath::RoundToInt(
				FMath::Sin(Angle) *
					BoundaryRadius);

		const int32 Height =
			Terrain->SampleMacro(
				X,
				Y).
				SurfaceZ;

		const bool bRiver =
			River->
				SampleNormalizedDistanceQ16(
					X,
					Y) <=
				65536;

		if (!bRiver &&
			Height <
				InFeature.WaterZ)
		{
			return false;
		}
	}

	return true;
}
