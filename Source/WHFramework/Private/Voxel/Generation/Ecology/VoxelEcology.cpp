#include "Voxel/Generation/Ecology/VoxelEcology.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

namespace
{
	constexpr uint64 TreeSalt = 0x62CEBFAD38B9C4D1ull;
	constexpr uint64 GrassSalt = 0x1987D4A6723FE901ull;
	constexpr uint64 TreeHeightSalt = 0x04C2A27F81DE930Bull;
	constexpr uint64 GrassFillSalt = 0x8D923F7415B7CA61ull;
	constexpr uint8 GrassPriority = 1;
	constexpr uint8 LeafPriority = 2;
	constexpr uint8 TrunkPriority = 3;
}

void FVoxelEcologyPlan::Finalize()
{
	InfluenceBounds.Init();
	ResolvedWrites.Reset();
	ResolvedWrites.Reserve(Writes.Num());

	for (const FVoxelEcologyPlanWrite& Write : Writes)
	{
		InfluenceBounds += FVector(Write.Position);
		InfluenceBounds += FVector(Write.Position) + FVector(1.0);
		FVoxelEcologyPlanWrite* Existing = ResolvedWrites.Find(Write.Position);
		if (!Existing || Write.Priority > Existing->Priority ||
			(Write.Priority == Existing->Priority && Existing->OwnerId < Write.OwnerId))
		{
			ResolvedWrites.Add(Write.Position, Write);
		}
	}
}

bool FVoxelEcologyPlan::Sample(const FIntVector& InPosition, uint32& OutValue) const
{
	const FVoxelEcologyPlanWrite* Write = ResolvedWrites.Find(InPosition);
	if (!Write)
	{
		return false;
	}
	OutValue = Write->Value;
	return true;
}

uint64 FVoxelEcologyPlan::GetAllocatedBytes() const
{
	return static_cast<uint64>(Writes.GetAllocatedSize()) +
		static_cast<uint64>(ResolvedWrites.GetAllocatedSize());
}

FVoxelEcologyGenerator::FVoxelEcologyGenerator(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe)
	: Recipe(InRecipe)
{
}

int32 FVoxelEcologyGenerator::EffectiveChance(
	const int32 InChancePermille,
	const int32 InDensityPermille)
{
	const int64 Scaled = static_cast<int64>(InChancePermille) * static_cast<int64>(InDensityPermille);
	return FMath::Clamp(static_cast<int32>(Scaled / 1000), 0, 1000);
}

bool FVoxelEcologyGenerator::IsTreeColumnAllowed(const FVoxelColumnSample& InColumn) const
{
	const FVoxelTreeGenerationSettings& Settings = Recipe->Settings.Ecology.Tree;
	if (InColumn.bOcean || InColumn.SlopePermille > Settings.MaxSlopePermille ||
		!Settings.Temperature.Contains(InColumn.Climate.TemperatureQ15) ||
		!Settings.Moisture.Contains(InColumn.Climate.MoistureQ15))
	{
		return false;
	}
	return Settings.bAllowNearWater || (!InColumn.bRiver && !InColumn.bLake && !InColumn.bCoast);
}

bool FVoxelEcologyGenerator::IsGrassColumnAllowed(const FVoxelColumnSample& InColumn) const
{
	const FVoxelGrassGenerationSettings& Settings = Recipe->Settings.Ecology.Grass;
	if (InColumn.bOcean || InColumn.SlopePermille > Settings.MaxSlopePermille ||
		!Settings.Temperature.Contains(InColumn.Climate.TemperatureQ15) ||
		!Settings.Moisture.Contains(InColumn.Climate.MoistureQ15))
	{
		return false;
	}
	return Settings.bAllowNearWater || (!InColumn.bRiver && !InColumn.bLake && !InColumn.bCoast);
}

void FVoxelEcologyGenerator::BuildTrees(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
	TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
	FVoxelEcologyPlan& InOutPlan,
	const TAtomic<bool>* InCancel) const
{
	EnumerateTrees(InBounds, InSampleColumn, InSampleBaseSymbol,
		[this, &InOutPlan](const FIntVector& Anchor, const int32 Height,
			const FVoxelStableId OwnerId)
		{
			for (int32 Z = 0; Z < Height; ++Z)
			{
				InOutPlan.Writes.Add({Anchor + FIntVector(0, 0, Z),
					Recipe->Ecology.TreeTrunk, TrunkPriority, OwnerId});
			}
			const int32 CrownRadius = Recipe->Settings.Ecology.Tree.CrownRadius;
			const FIntVector CrownCenter = Anchor + FIntVector(0, 0, Height - 1);
			for (int32 Z = -CrownRadius; Z <= CrownRadius; ++Z)
			{
				for (int32 Y = -CrownRadius; Y <= CrownRadius; ++Y)
				{
					for (int32 X = -CrownRadius; X <= CrownRadius; ++X)
					{
						if (X * X + Y * Y + Z * Z > CrownRadius * CrownRadius)
						{
							continue;
						}
						const FIntVector Position = CrownCenter + FIntVector(X, Y, Z);
						if (X == 0 && Y == 0 && Position.Z >= Anchor.Z &&
							Position.Z < Anchor.Z + Height)
						{
							continue;
						}
						InOutPlan.Writes.Add({Position, Recipe->Ecology.TreeLeaves,
							LeafPriority, OwnerId});
					}
				}
			}
		}, InOutPlan.TreeCandidates, InOutPlan.TreesAccepted, InCancel);
}

void FVoxelEcologyGenerator::EnumerateTrees(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
	TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
	TFunctionRef<void(const FIntVector&, int32, FVoxelStableId)> InVisit,
	int32& OutCandidates,
	int32& OutAccepted,
	const TAtomic<bool>* InCancel) const
{
	const FVoxelTreeGenerationSettings& Settings = Recipe->Settings.Ecology.Tree;
	const int32 Chance = EffectiveChance(Settings.ChancePermille, Settings.DensityPermille);
	if (!Settings.bEnabled || Chance <= 0)
	{
		return;
	}

	const int32 Spacing = FMath::Max(4, Settings.Spacing);
	const int32 MinGridX = VoxelGeneration::FloorDivide(InBounds.Min.X, Spacing);
	const int32 MaxGridX = VoxelGeneration::FloorDivide(InBounds.Max.X - 1, Spacing);
	const int32 MinGridY = VoxelGeneration::FloorDivide(InBounds.Min.Y, Spacing);
	const int32 MaxGridY = VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, Spacing);

	for (int32 GridY = MinGridY; GridY <= MaxGridY; ++GridY)
	{
		for (int32 GridX = MinGridX; GridX <= MaxGridX; ++GridX)
		{
			++OutCandidates;
			if (InCancel && InCancel->Load())
			{
				return;
			}

			const FIntVector Grid(GridX, GridY, 0);
			const uint64 CandidateSeed = VoxelGeneration::MakeSeed(Recipe->Settings.Seed, Grid, TreeSalt);
			const int32 OffsetX = VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(CandidateSeed ^ 0x91827211ABF49D33ull), 0, Spacing - 1);
			const int32 OffsetY = VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(CandidateSeed ^ 0x42CF9A177A16C593ull), 0, Spacing - 1);
			const FIntVector XY(GridX * Spacing + OffsetX, GridY * Spacing + OffsetY, 0);
			if (XY.X < InBounds.Min.X || XY.X >= InBounds.Max.X ||
				XY.Y < InBounds.Min.Y || XY.Y >= InBounds.Max.Y)
			{
				continue;
			}

			FVoxelColumnSample Column;
			if (!InSampleColumn(XY, Column) || !IsTreeColumnAllowed(Column))
			{
				continue;
			}

			const FIntVector Anchor(XY.X, XY.Y, Column.SurfaceZ + 1);
			if (VoxelGeneration::RandomRange(VoxelGeneration::Mix(CandidateSeed), 0, 999) >= Chance)
			{
				continue;
			}

			uint32 AirValue = MAX_uint32;
			uint32 FloorValue = MAX_uint32;
			if (!InSampleBaseSymbol(Anchor, AirValue) ||
				!InSampleBaseSymbol(Anchor - FIntVector(0, 0, 1), FloorValue) ||
				static_cast<uint16>(AirValue & 0xffffu) != Recipe->Palette.Air ||
				static_cast<uint16>(FloorValue & 0xffffu) == Recipe->Palette.Air)
			{
				continue;
			}

			const FVoxelStableId OwnerId = VoxelGeneration::MakeStableId(
				Recipe->Settings.Seed, Anchor, TreeSalt);
			++OutAccepted;
			const int32 Height = VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(CandidateSeed ^ TreeHeightSalt), Settings.MinHeight, Settings.MaxHeight);
			InVisit(Anchor, Height, OwnerId);
		}
	}
}

void FVoxelEcologyGenerator::BuildGrass(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector&, FVoxelColumnSample&)> InSampleColumn,
	TFunctionRef<bool(const FIntVector&, uint32&)> InSampleBaseSymbol,
	FVoxelEcologyPlan& InOutPlan,
	const TAtomic<bool>* InCancel) const
{
	const FVoxelGrassGenerationSettings& Settings = Recipe->Settings.Ecology.Grass;
	const int32 Chance = EffectiveChance(Settings.ChancePermille, Settings.DensityPermille);
	if (!Settings.bEnabled || Chance <= 0)
	{
		return;
	}

	const int32 Spacing = FMath::Max(2, Settings.Spacing);
	const int32 MinGridX = VoxelGeneration::FloorDivide(InBounds.Min.X, Spacing);
	const int32 MaxGridX = VoxelGeneration::FloorDivide(InBounds.Max.X - 1, Spacing);
	const int32 MinGridY = VoxelGeneration::FloorDivide(InBounds.Min.Y, Spacing);
	const int32 MaxGridY = VoxelGeneration::FloorDivide(InBounds.Max.Y - 1, Spacing);

	for (int32 GridY = MinGridY; GridY <= MaxGridY; ++GridY)
	{
		for (int32 GridX = MinGridX; GridX <= MaxGridX; ++GridX)
		{
			if (InCancel && InCancel->Load())
			{
				return;
			}
			const FIntVector Grid(GridX, GridY, 0);
			const uint64 CandidateSeed = VoxelGeneration::MakeSeed(Recipe->Settings.Seed, Grid, GrassSalt);
			const int32 OffsetX = VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(CandidateSeed ^ 0xFA9170D133E38A21ull), 0, Spacing - 1);
			const int32 OffsetY = VoxelGeneration::RandomRange(
				VoxelGeneration::Mix(CandidateSeed ^ 0x5A738CB14268F9E1ull), 0, Spacing - 1);
			const FIntVector PatchCenter(GridX * Spacing + OffsetX, GridY * Spacing + OffsetY, 0);
			++InOutPlan.GrassPatchCandidates;
			if (PatchCenter.X < InBounds.Min.X || PatchCenter.X >= InBounds.Max.X ||
				PatchCenter.Y < InBounds.Min.Y || PatchCenter.Y >= InBounds.Max.Y ||
				VoxelGeneration::RandomRange(VoxelGeneration::Mix(CandidateSeed), 0, 999) >= Chance)
			{
				continue;
			}

			const FVoxelStableId OwnerId = VoxelGeneration::MakeStableId(
				Recipe->Settings.Seed, PatchCenter, GrassSalt);
			for (int32 Y = -Settings.PatchRadius; Y <= Settings.PatchRadius; ++Y)
			{
				for (int32 X = -Settings.PatchRadius; X <= Settings.PatchRadius; ++X)
				{
					if (InCancel && InCancel->Load())
					{
						return;
					}
					const FIntVector XY(PatchCenter.X + X, PatchCenter.Y + Y, 0);
					FVoxelColumnSample Column;
					if (!InSampleColumn(XY, Column) || !IsGrassColumnAllowed(Column))
					{
						continue;
					}
					const FIntVector SurfacePosition(XY.X, XY.Y, Column.SurfaceZ + 1);
					const uint64 PointSeed = VoxelGeneration::MakeSeed(
						Recipe->Settings.Seed, SurfacePosition, GrassFillSalt);
					if (VoxelGeneration::RandomRange(PointSeed, 0, 999) >= Settings.PatchFillPermille)
					{
						continue;
					}
					uint32 AirValue = MAX_uint32;
					uint32 FloorValue = MAX_uint32;
					if (!InSampleBaseSymbol(SurfacePosition, AirValue) ||
						!InSampleBaseSymbol(SurfacePosition - FIntVector(0, 0, 1), FloorValue) ||
						static_cast<uint16>(AirValue & 0xffffu) != Recipe->Palette.Air ||
						static_cast<uint16>(FloorValue & 0xffffu) == Recipe->Palette.Air)
					{
						continue;
					}
					InOutPlan.Writes.Add({SurfacePosition, Recipe->Ecology.GrassPlant, GrassPriority, OwnerId});
					++InOutPlan.GrassWrites;
				}
			}
		}
	}
}

bool FVoxelEcologyGenerator::BuildPlan(
	const FVoxelGenerationBounds& InBounds,
	TFunctionRef<bool(const FIntVector& InPosition, FVoxelColumnSample& OutColumn)> InSampleColumn,
	TFunctionRef<bool(const FIntVector& InPosition, uint32& OutBaseSymbol)> InSampleBaseSymbol,
	FVoxelEcologyPlan& OutPlan,
	FString& OutError,
	const TAtomic<bool>* InCancel) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Voxel_EcologyPlan);
	if (!InBounds.IsValid())
	{
		OutError = TEXT("Voxel ecology planner received invalid bounds");
		return false;
	}

	FVoxelEcologyPlan Plan;
	Plan.Bounds = InBounds;
	BuildTrees(InBounds, InSampleColumn, InSampleBaseSymbol, Plan, InCancel);
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	BuildGrass(InBounds, InSampleColumn, InSampleBaseSymbol, Plan, InCancel);
	if (InCancel && InCancel->Load())
	{
		OutError = TEXT("Canceled");
		return false;
	}
	Plan.Finalize();

#if !UE_BUILD_SHIPPING
	UE_LOG(
		LogTemp,
		VeryVerbose,
		TEXT("Voxel ecology bounds=(%d,%d)-(%d,%d) tree=%d/%d grassPatches=%d grassWrites=%d totalWrites=%d"),
		InBounds.Min.X,
		InBounds.Min.Y,
		InBounds.Max.X,
		InBounds.Max.Y,
		Plan.TreesAccepted,
		Plan.TreeCandidates,
		Plan.GrassPatchCandidates,
		Plan.GrassWrites,
		Plan.Writes.Num());
#endif

	OutPlan = MoveTemp(Plan);
	OutError.Reset();
	return true;
}
