#include "Voxel/Generation/VoxelGenerationBinding.h"

#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"

bool FVoxelGenerationRuntimeConfig::IsValid() const
{
	return Recipe.IsValid() && !SymbolToRuntime.IsEmpty() && !RuntimeToSymbol.IsEmpty();
}

bool FVoxelGenerationRuntimeConfig::ToRuntime(uint32 InPackedSymbol, FVoxelBlockState& OutState) const
{
	const uint16 Symbol = static_cast<uint16>(InPackedSymbol & 0xffffu);
	if (!SymbolToRuntime.IsValidIndex(Symbol))
	{
		return false;
	}
	OutState.TypeId = SymbolToRuntime[Symbol];
	OutState.State = static_cast<uint16>(InPackedSymbol >> 16);
	return true;
}

bool FVoxelGenerationRuntimeConfig::ToSymbol(FVoxelBlockState InState, uint32& OutPackedSymbol) const
{
	if (!RuntimeToSymbol.IsValidIndex(InState.TypeId))
	{
		return false;
	}
	const uint16 Symbol = RuntimeToSymbol[InState.TypeId];
	if (Symbol == MAX_uint16)
	{
		return false;
	}
	OutPackedSymbol = static_cast<uint32>(Symbol) | (static_cast<uint32>(InState.State) << 16);
	return true;
}

bool FVoxelGenerationBinding::Build(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
	const FVoxelGenerationSettings& InSettings, int32 InCellCentimeters,
	FVoxelGenerationRuntimeConfig& OutConfig, FString& OutError)
{
	if (InProfile.RecipeBytes.IsEmpty())
	{
		OutError = TEXT("Voxel generation profile has no baked recipe");
		return false;
	}
	if (InCellCentimeters <= 0 || !InSettings.Validate(OutError))
	{
		if (OutError.IsEmpty()) OutError = TEXT("Voxel runtime cell size must be positive");
		return false;
	}

	FVoxelGenerationRecipe RuntimeRecipe;
	if (!FVoxelGenerationRecipeCodec::Decode(InProfile.RecipeBytes, RuntimeRecipe, OutError))
	{
		return false;
	}
	if (RuntimeRecipe.RecipeHash != InProfile.RecipeHash)
	{
		OutError = TEXT("Voxel generation profile RecipeHash does not match RecipeBytes");
		return false;
	}
	if (RuntimeRecipe.CellCentimeters != InProfile.BakedCellCentimeters || RuntimeRecipe.CellCentimeters != InCellCentimeters)
	{
		OutError = TEXT("Voxel generation profile bake cell size does not match runtime cell size");
		return false;
	}
	RuntimeRecipe.Settings = InSettings;
	if (!FVoxelGenerationRecipeCodec::RefreshHash(RuntimeRecipe, OutError) || !RuntimeRecipe.BuildLookups(OutError))
	{
		return false;
	}

	FVoxelGenerationRuntimeConfig Result;
	Result.RegistryHash = InRegistry.Hash;
	Result.SymbolToRuntime.SetNum(RuntimeRecipe.BlockNames.Num());
	Result.RuntimeToSymbol.Init(MAX_uint16, InRegistry.Definitions.Num());
	Result.SymbolToRuntime[0] = VoxelBlock::Air;
	if (Result.RuntimeToSymbol.IsValidIndex(VoxelBlock::Air))
	{
		Result.RuntimeToSymbol[VoxelBlock::Air] = 0;
	}

	for (int32 SymbolIndex = 1; SymbolIndex < RuntimeRecipe.BlockNames.Num(); ++SymbolIndex)
	{
		const FName BlockName = RuntimeRecipe.BlockNames[SymbolIndex];
		const FVoxelRuntimeDefinition* Definition = InRegistry.Find(BlockName);
		if (!Definition)
		{
			OutError = FString::Printf(TEXT("Voxel recipe references an unregistered block: %s"), *BlockName.ToString());
			return false;
		}
		if (!Result.RuntimeToSymbol.IsValidIndex(Definition->TypeId))
		{
			OutError = TEXT("Voxel runtime block id exceeds generation binding table");
			return false;
		}
		Result.SymbolToRuntime[SymbolIndex] = Definition->TypeId;
		Result.RuntimeToSymbol[Definition->TypeId] = static_cast<uint16>(SymbolIndex);
	}

	auto Resolve = [&Result, &OutError](uint16 Symbol, bool bRequired, FVoxelBlockState& OutState)
	{
		if (Symbol == MAX_uint16)
		{
			if (bRequired)
			{
				OutError = TEXT("Voxel recipe is missing a required palette symbol");
				return false;
			}
			OutState = {};
			return true;
		}
		if (!Result.SymbolToRuntime.IsValidIndex(Symbol))
		{
			OutError = TEXT("Voxel recipe palette symbol exceeds runtime binding table");
			return false;
		}
		OutState = FVoxelBlockState(Result.SymbolToRuntime[Symbol], 0);
		return true;
	};

	Result.Air = {};
	if (!Resolve(RuntimeRecipe.Palette.Stone, true, Result.Stone) ||
		!Resolve(RuntimeRecipe.Palette.Dirt, true, Result.Dirt) ||
		!Resolve(RuntimeRecipe.Palette.Grass, true, Result.Grass) ||
		!Resolve(RuntimeRecipe.Palette.Sand, true, Result.Sand) ||
		!Resolve(RuntimeRecipe.Palette.Snow, true, Result.Snow) ||
		!Resolve(RuntimeRecipe.Palette.Water, true, Result.Water) ||
		!Resolve(RuntimeRecipe.Palette.Lava, true, Result.Lava) ||
		!Resolve(RuntimeRecipe.Palette.Bedrock, true, Result.Bedrock) ||
		!Resolve(RuntimeRecipe.Palette.Road, false, Result.Road))
	{
		return false;
	}

	Result.Recipe = MakeShared<const FVoxelGenerationRecipe, ESPMode::ThreadSafe>(MoveTemp(RuntimeRecipe));
	OutConfig = MoveTemp(Result);
	OutError.Reset();
	return true;
}
