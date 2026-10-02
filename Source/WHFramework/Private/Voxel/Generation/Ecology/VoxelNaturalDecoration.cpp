#include "Voxel/Generation/Ecology/VoxelNaturalDecoration.h"

#include "Voxel/Generation/VoxelGenerationMath.h"

uint32 FVoxelNaturalDecoration::Resolve(const FVoxelGenerationRecipe& InRecipe, const FVoxelColumnSample& InColumn,
	const FIntVector& InPosition, const uint32 InCurrentSymbol,
	TFunctionRef<bool(const FIntVector&)> InIsCavity, TFunctionRef<bool(const FIntVector&)> InIsProtected)
{
	if (InRecipe.Ecology.NaturalDecorations.IsEmpty() || InPosition.Z > InColumn.SurfaceZ - 8 || InIsProtected(InPosition)) return InCurrentSymbol;
	auto Symbol = [&](const FName Role) -> uint32
	{
		const uint16* Value = InRecipe.Ecology.NaturalDecorations.Find(Role);
		return Value ? *Value : InCurrentSymbol;
	};
	const bool bWet = InColumn.Climate.MoistureQ15 > 0;
	if (static_cast<uint16>(InCurrentSymbol) == InRecipe.Palette.Stone && bWet)
	{
		for (const FIntVector Direction : { FIntVector(1, 0, 0), FIntVector(-1, 0, 0), FIntVector(0, 1, 0), FIntVector(0, -1, 0), FIntVector(0, 0, 1) })
		{
			if (InIsCavity(InPosition + Direction)) return Symbol(TEXT("WetRock"));
		}
		return InCurrentSymbol;
	}
	if (static_cast<uint16>(InCurrentSymbol) != InRecipe.Palette.Air || !InIsCavity(InPosition)) return InCurrentSymbol;
	int32 FloorDistance = 0;
	int32 CeilingDistance = 0;
	while (FloorDistance < 16 && InIsCavity(InPosition - FIntVector(0, 0, FloorDistance + 1))) ++FloorDistance;
	while (CeilingDistance < 16 && InIsCavity(InPosition + FIntVector(0, 0, CeilingDistance + 1))) ++CeilingDistance;
	const int32 Clearance = FloorDistance + CeilingDistance + 1;
	const FIntVector Floor(InPosition.X, InPosition.Y, InPosition.Z - FloorDistance);
	if (FloorDistance == 16 || CeilingDistance == 16 || InIsProtected(Floor)) return InCurrentSymbol;
	const uint64 Seed = VoxelGeneration::MakeSeed(InRecipe.Settings.Seed, Floor, 0x434156454445434full);
	// Collision-bearing formations need a wide cavity and leave at least two metres of clear headroom.
	const bool bWide = Clearance >= 12 && InIsCavity(Floor + FIntVector(2, 0, 2)) && InIsCavity(Floor + FIntVector(-2, 0, 2)) &&
		InIsCavity(Floor + FIntVector(0, 2, 2)) && InIsCavity(Floor + FIntVector(0, -2, 2));
	const int32 FormationLength = FMath::Min(3, (Clearance - 8) / 2);
	if (bWide && (Seed % 47) == 0 && CeilingDistance < FormationLength) return Symbol(TEXT("Stalactite"));
	if (bWide && (Seed % 53) == 0 && FloorDistance < FormationLength) return Symbol(TEXT("Stalagmite"));
	if (bWet && FloorDistance == 0 && Clearance >= 4 && (Seed % 19) == 0)
	{
		return Symbol((Seed & 2) ? FName(TEXT("Mushroom")) : FName(TEXT("UndergroundPlant")));
	}
	return InCurrentSymbol;
}
