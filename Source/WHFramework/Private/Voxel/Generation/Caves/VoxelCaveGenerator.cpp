#include "Voxel/Generation/Caves/VoxelCaveGenerator.h"

#include "Voxel/Generation/Hydrology/VoxelHydrology.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Generation/VoxelGenerationMath.h"

bool FVoxelCaveRoute::Carves(const FIntVector& InCell) const
{
	for (const FVoxelCaveRouteSection& Section : Sections)
	{
		if (InCell.Z < Section.FloorZ || InCell.Z > Section.CeilingZ)
		{
			continue;
		}
		const int64 DX = static_cast<int64>(InCell.X) - Section.Center.X;
		const int64 DY = static_cast<int64>(InCell.Y) - Section.Center.Y;
		if (DX * DX + DY * DY <= static_cast<int64>(Section.Radius) * Section.Radius)
		{
			return true;
		}
	}
	return false;
}

bool FVoxelCaveRoute::ProtectsFloor(const FIntVector& InCell) const
{
	for (const FVoxelCaveRouteSection& Section : Sections)
	{
		if (InCell.Z != Section.FloorZ - 1)
		{
			continue;
		}
		const int64 DX = static_cast<int64>(InCell.X) - Section.Center.X;
		const int64 DY = static_cast<int64>(InCell.Y) - Section.Center.Y;
		if (DX * DX + DY * DY <= static_cast<int64>(Section.Radius) * Section.Radius)
		{
			return true;
		}
	}
	return false;
}

bool FVoxelCavePlan::Carves(const FIntVector& InCell) const
{
	for (const FVoxelCaveRoute& Route : Routes)
	{
		if (Route.Carves(InCell))
		{
			return true;
		}
	}
	for (const FVoxelCaveChamber& Chamber : Chambers)
	{
		if (VoxelGeneration::IsInsideEllipsoid(InCell, Chamber.Center, Chamber.Radius))
		{
			return true;
		}
	}
	return false;
}

bool FVoxelCavePlan::ProtectsFloor(const FIntVector& InCell) const
{
	for (const FVoxelCaveRoute& Route : Routes)
	{
		if (Route.ProtectsFloor(InCell))
		{
			return true;
		}
	}
	return false;
}

uint64 FVoxelCavePlan::GetAllocatedBytes() const
{
	uint64 Bytes = Routes.GetAllocatedSize() + Chambers.GetAllocatedSize();
	for (const FVoxelCaveRoute& Route : Routes)
	{
		Bytes += Route.Sections.GetAllocatedSize();
	}
	return Bytes;
}

FVoxelCaveGenerator::FVoxelCaveGenerator(
	TSharedRef<const FVoxelGenerationRecipe, ESPMode::ThreadSafe> InRecipe,
	TSharedRef<const FVoxelTerrainGenerator, ESPMode::ThreadSafe> InTerrain,
	TSharedRef<const FVoxelHydrologyGenerator, ESPMode::ThreadSafe> InHydrology)
	: Recipe(InRecipe)
	, Terrain(InTerrain)
	, Hydrology(InHydrology)
{
}

bool FVoxelCaveGenerator::BuildPlan(const FVoxelGenerationBounds& InBounds, FVoxelCavePlan& OutPlan,
	FString& OutError, const TAtomic<bool>* InCancel) const
{
	TArray<FVoxelCaveAnchor> Anchors;
	GatherAnchors(InBounds, Anchors);
	FVoxelCavePlan Result;
	for (const FVoxelCaveAnchor& Anchor : Anchors)
	{
		if (InCancel && InCancel->Load())
		{
			OutError = TEXT("Canceled");
			return false;
		}
		FVoxelCaveRoute MainRoute;
		if (!BuildMainRoute(Anchor, MainRoute, OutError))
		{
			return false;
		}
		Result.Routes.Add(MainRoute);
		AddBranches(Anchor, MainRoute, Result.Routes);
		AddChambers(Anchor, MainRoute, Result.Chambers);
	}
	Result.Routes.Sort([](const FVoxelCaveRoute& A, const FVoxelCaveRoute& B) { return A.Id < B.Id; });
	Result.Chambers.Sort([](const FVoxelCaveChamber& A, const FVoxelCaveChamber& B) { return A.Id < B.Id; });
	OutPlan = MoveTemp(Result);
	OutError.Reset();
	return true;
}

void FVoxelCaveGenerator::GatherAnchors(const FVoxelGenerationBounds& InBounds, TArray<FVoxelCaveAnchor>& OutAnchors) const
{
	const int32 Spacing = Recipe->Settings.CaveSpacing;
	const int32 Margin = Recipe->Settings.CaveMaxDepth + Spacing;
	const int32 MinGridX = VoxelGeneration::FloorDivide(InBounds.Min.X - Margin, Spacing);
	const int32 MaxGridX = VoxelGeneration::FloorDivide(InBounds.Max.X + Margin - 1, Spacing);
	const int32 MinGridY = VoxelGeneration::FloorDivide(InBounds.Min.Y - Margin, Spacing);
	const int32 MaxGridY = VoxelGeneration::FloorDivide(InBounds.Max.Y + Margin - 1, Spacing);
	for (int32 GridY = MinGridY; GridY <= MaxGridY; ++GridY)
	{
		for (int32 GridX = MinGridX; GridX <= MaxGridX; ++GridX)
		{
			const FIntVector Grid(GridX, GridY, 0);
			const uint64 Seed = VoxelGeneration::MakeSeed(Recipe->Settings.Seed, Grid, 0xCA7E42E1A93411D3ull);
			if (VoxelGeneration::RandomRange(Seed, 0, 999) >= 420)
			{
				continue;
			}
			const int32 WorldX = GridX * Spacing + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0xA019BC27ull), Spacing / 5, Spacing * 4 / 5);
			const int32 WorldY = GridY * Spacing + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x4ECA31D9ull), Spacing / 5, Spacing * 4 / 5);
			const FVoxelMacroTerrainSample TerrainSample = Terrain->SampleMacro(WorldX, WorldY);
			if (TerrainSample.SurfaceZ <= Recipe->Settings.SeaLevel + 4)
			{
				continue;
			}
			const int32 Depth = VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x71A4F0E5ull),
				Recipe->Settings.CaveMinDepth, Recipe->Settings.CaveMaxDepth);
			FVoxelCaveAnchor Anchor;
			Anchor.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed, Grid, 0xCA7E42E1A93411D3ull);
			Anchor.Entrance = FIntVector(WorldX, WorldY, TerrainSample.SurfaceZ + 1);
			Anchor.Target = FIntVector(
				WorldX + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x8334B64Full), -Spacing / 2, Spacing / 2),
				WorldY + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x13C65D2Bull), -Spacing / 2, Spacing / 2),
				FMath::Max(Recipe->Settings.MinZ + 16, TerrainSample.SurfaceZ - Depth));
			Anchor.Radius = Recipe->Settings.CaveMainRadius;
			OutAnchors.Add(Anchor);
		}
	}
}

bool FVoxelCaveGenerator::BuildMainRoute(const FVoxelCaveAnchor& InAnchor, FVoxelCaveRoute& OutRoute, FString& OutError) const
{
	const FIntVector Delta = InAnchor.Target - InAnchor.Entrance;
	const int32 Length = FMath::Max3(FMath::Abs(Delta.X), FMath::Abs(Delta.Y), FMath::Abs(Delta.Z));
	const int32 MaxVerticalStep = FMath::Max(2, InAnchor.Radius);
	const int32 Steps = FMath::Clamp(FMath::DivideAndRoundUp(Length, MaxVerticalStep), 4, 256);
	FVoxelCaveRoute Route;
	Route.Id = InAnchor.Id;
	Route.Sections.Reserve(Steps + 1);
	for (int32 Step = 0; Step <= Steps; ++Step)
	{
		FIntVector Center(
			InAnchor.Entrance.X + static_cast<int32>(static_cast<int64>(Delta.X) * Step / Steps),
			InAnchor.Entrance.Y + static_cast<int32>(static_cast<int64>(Delta.Y) * Step / Steps),
			InAnchor.Entrance.Z + static_cast<int32>(static_cast<int64>(Delta.Z) * Step / Steps));
		const uint64 JitterSeed = VoxelGeneration::Mix(InAnchor.Id.High ^ static_cast<uint64>(Step) * 0x9E3779B97F4A7C15ull);
		if (Step > 0 && Step < Steps)
		{
			Center.X += VoxelGeneration::RandomRange(JitterSeed, -8, 8);
			Center.Y += VoxelGeneration::RandomRange(VoxelGeneration::Mix(JitterSeed), -8, 8);
		}
		const int32 Radius = FMath::Max(2, InAnchor.Radius + VoxelGeneration::RandomRange(
			VoxelGeneration::Mix(JitterSeed ^ 0x93A1D7F2ull), 0, 3));
		FVoxelCaveRouteSection& Section = Route.Sections.AddDefaulted_GetRef();
		Section.Center = Center;
		Section.Radius = Radius;
		Section.FloorZ = Center.Z - Radius / 2;
		Section.CeilingZ = Center.Z + FMath::Max(2, Radius);
	}
	if (!ValidateWalkRoute(Route, InAnchor.Entrance, InAnchor.Target, OutError))
	{
		return false;
	}
	OutRoute = MoveTemp(Route);
	return true;
}

void FVoxelCaveGenerator::AddBranches(const FVoxelCaveAnchor& InAnchor, const FVoxelCaveRoute& InMainRoute,
	TArray<FVoxelCaveRoute>& OutRoutes) const
{
	if (InMainRoute.Sections.Num() < 5)
	{
		return;
	}
	const int32 BranchCount = VoxelGeneration::RandomRange(VoxelGeneration::Mix(InAnchor.Id.Low), 0, 3);
	for (int32 BranchIndex = 0; BranchIndex < BranchCount; ++BranchIndex)
	{
		const uint64 Seed = VoxelGeneration::Mix(InAnchor.Id.High ^ static_cast<uint64>(BranchIndex + 1) * 0xD6E8FEB86659FD93ull);
		const int32 StartIndex = VoxelGeneration::RandomRange(Seed, 2, InMainRoute.Sections.Num() - 3);
		const FIntVector Start = InMainRoute.Sections[StartIndex].Center;
		const int32 BranchLength = VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed), 48, 160);
		FIntVector End(
			Start.X + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x195CE13Full), -BranchLength, BranchLength),
			Start.Y + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x2C1B3C6Dull), -BranchLength, BranchLength),
			FMath::Clamp(Start.Z + VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x71388437ull), -BranchLength / 3, BranchLength / 3),
				Recipe->Settings.MinZ + 8, Start.Z + 32));
		FVoxelCaveRoute Branch;
		Branch.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed, Start, InAnchor.Id.Low, BranchIndex + 1);
		const FIntVector Centers[] = {Start, (Start + End) / 2, End};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Centers); ++Index)
		{
			FVoxelCaveRouteSection& Section = Branch.Sections.AddDefaulted_GetRef();
			Section.Center = Centers[Index];
			Section.Radius = FMath::Max(2, Recipe->Settings.CaveBranchRadius - Index / 2);
			Section.FloorZ = Section.Center.Z - Section.Radius / 2;
			Section.CeilingZ = Section.Center.Z + FMath::Max(2, Section.Radius);
		}
		OutRoutes.Add(MoveTemp(Branch));
	}
}

void FVoxelCaveGenerator::AddChambers(const FVoxelCaveAnchor& InAnchor, const FVoxelCaveRoute& InMainRoute,
	TArray<FVoxelCaveChamber>& OutChambers) const
{
	if (InMainRoute.Sections.Num() < 4)
	{
		return;
	}
	const int32 Count = VoxelGeneration::RandomRange(VoxelGeneration::Mix(InAnchor.Id.High ^ 0x31B56A93ull), 1, 3);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const uint64 Seed = VoxelGeneration::Mix(InAnchor.Id.Low ^ static_cast<uint64>(Index + 1) * 0x94D049BB133111EBull);
		const int32 SectionIndex = VoxelGeneration::RandomRange(Seed, 1, InMainRoute.Sections.Num() - 2);
		FVoxelCaveChamber Chamber;
		Chamber.Center = InMainRoute.Sections[SectionIndex].Center;
		Chamber.Id = VoxelGeneration::MakeStableId(Recipe->Settings.Seed, Chamber.Center, InAnchor.Id.High, Index + 1);
		Chamber.Radius = FIntVector(
			VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed), Recipe->Settings.CaveMainRadius, Recipe->Settings.CaveMainRadius * 3),
			VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0x73A48F19ull), Recipe->Settings.CaveMainRadius, Recipe->Settings.CaveMainRadius * 3),
			VoxelGeneration::RandomRange(VoxelGeneration::Mix(Seed ^ 0xC124D7A3ull), Recipe->Settings.CaveMainRadius / 2, Recipe->Settings.CaveMainRadius * 2));
		OutChambers.Add(Chamber);
	}
}

bool FVoxelCaveGenerator::ValidateWalkRoute(const FVoxelCaveRoute& InRoute, const FIntVector& InEntrance,
	const FIntVector& InTarget, FString& OutError) const
{
	if (InRoute.Sections.Num() < 2 || InRoute.Sections[0].Center != InEntrance || InRoute.Sections.Last().Center != InTarget)
	{
		OutError = TEXT("Cave route endpoints are invalid");
		return false;
	}
	for (int32 Index = 0; Index < InRoute.Sections.Num(); ++Index)
	{
		const FVoxelCaveRouteSection& Section = InRoute.Sections[Index];
		if (Section.Radius < 2 || Section.CeilingZ - Section.FloorZ < 2 || Section.FloorZ <= Recipe->Settings.MinZ)
		{
			OutError = TEXT("Cave route section lacks required walkable headroom or floor protection");
			return false;
		}
		if (Index > 0)
		{
			const FIntVector Delta = Section.Center - InRoute.Sections[Index - 1].Center;
			if (FMath::Abs(Delta.Z) > FMath::Max(FMath::Abs(Delta.X), FMath::Abs(Delta.Y)) + Section.Radius)
			{
				OutError = TEXT("Cave route contains an unwalkable vertical segment");
				return false;
			}
		}
	}
	OutError.Reset();
	return true;
}
