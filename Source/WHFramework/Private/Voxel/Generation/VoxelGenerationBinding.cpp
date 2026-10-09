#include "Voxel/Generation/VoxelGenerationBinding.h"

#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"
#include "Voxel/Generation/VoxelGenerationAllocation.h"

namespace
{
	bool AddFrozenGenerationBytes(uint64& InOutBytes, const uint64 InBytes)
	{
		if (InBytes > MAX_uint64 - InOutBytes)
		{
			return false;
		}
		InOutBytes += InBytes;
		return true;
	}

	template<typename ElementType>
	uint64 FrozenGenerationArrayBytes(const TArray<ElementType>& InArray, const bool bInClone = true)
	{
		return bInClone ? VoxelGenerationAllocation::Private::ArrayReserveBytes<ElementType, FDefaultAllocator>(InArray.Num()) :
			VoxelGenerationAllocation::AllocatedBytes(InArray);
	}

	template<typename KeyType, typename ValueType>
	uint64 FrozenGenerationMapBytes(const TMap<KeyType, ValueType>& InMap, const bool bInClone = true)
	{
		if (!bInClone)
		{
			return VoxelGenerationAllocation::AllocatedBytes(InMap);
		}
		const TMap<KeyType, ValueType> Empty;
		return VoxelGenerationAllocation::Private::SetReserveBytes<TPair<KeyType, ValueType>>(Empty, InMap.Num());
	}

	template<typename ElementType>
	void CopyFrozenGenerationArray(const TArray<ElementType>& InArray, TArray<ElementType>& OutArray)
	{
		OutArray.Reserve(InArray.Num());
		OutArray.Append(InArray.GetData(), InArray.Num());
	}

	template<typename KeyType, typename ValueType>
	void CopyFrozenGenerationMap(const TMap<KeyType, ValueType>& InMap, TMap<KeyType, ValueType>& OutMap)
	{
		OutMap.Reserve(InMap.Num());
		for (const auto& Pair : InMap)
		{
			OutMap.Add(Pair.Key, Pair.Value);
		}
	}

	// Count 与实际复制使用相同数组路径；嵌套数组的空目标 copy constructor 只分配 Num。
	bool CountFrozenGenerationRecipe(const FVoxelGenerationRecipe& InRecipe, uint64& OutBytes, const bool bInClone = true)
	{
		OutBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelGenerationRecipe>();
		const uint64 FlatBytes[] = {
			FrozenGenerationArrayBytes(InRecipe.BlockNames, bInClone), FrozenGenerationArrayBytes(InRecipe.Ecology.Flowers, bInClone),
			FrozenGenerationArrayBytes(InRecipe.Ecology.TreeSpecies, bInClone), FrozenGenerationMapBytes(InRecipe.Ecology.NaturalDecorations, bInClone),
			FrozenGenerationArrayBytes(InRecipe.Biomes, bInClone), FrozenGenerationArrayBytes(InRecipe.Features, bInClone),
			FrozenGenerationArrayBytes(InRecipe.Structures, bInClone), FrozenGenerationArrayBytes(InRecipe.SurfaceRules, bInClone),
			FrozenGenerationMapBytes(InRecipe.BlockLookup, bInClone), FrozenGenerationMapBytes(InRecipe.BiomeLookup, bInClone),
			FrozenGenerationMapBytes(InRecipe.FeatureLookup, bInClone), FrozenGenerationMapBytes(InRecipe.StructureLookup, bInClone),
			FrozenGenerationMapBytes(InRecipe.SurfaceRuleLookup, bInClone)
		};
		for (const uint64 Bytes : FlatBytes)
		{
			if (!AddFrozenGenerationBytes(OutBytes, Bytes)) return false;
		}
		for (const auto& Biome : InRecipe.Biomes)
		{
			if (!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Biome.FeatureIndices, bInClone)) ||
				!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Biome.StructureIndices, bInClone))) return false;
		}
		for (const auto& Feature : InRecipe.Features)
		{
			if (!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Feature.ConfigBytes, bInClone))) return false;
		}
		for (const auto& Structure : InRecipe.Structures)
		{
			if (!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Structure.Pieces, bInClone))) return false;
			for (const auto& Piece : Structure.Pieces)
			{
				if (!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Piece.Writes, bInClone)) ||
					!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Piece.ClearVolumes, bInClone)) ||
					!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Piece.Connectors, bInClone)) ||
					!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Piece.Details, bInClone))) return false;
			}
		}
		for (const auto& Rules : InRecipe.SurfaceRules)
		{
			if (!AddFrozenGenerationBytes(OutBytes, FrozenGenerationArrayBytes(Rules.Rules, bInClone))) return false;
		}
		return true;
	}

	void CopyFrozenGenerationRecipe(const FVoxelGenerationRecipe& InSource, FVoxelGenerationRecipe& OutRecipe)
	{
		OutRecipe.AlgorithmVersion = InSource.AlgorithmVersion;
		OutRecipe.RecipeHash = InSource.RecipeHash;
		OutRecipe.CellCentimeters = InSource.CellCentimeters;
		OutRecipe.Settings = InSource.Settings;
		OutRecipe.Palette = InSource.Palette;
		OutRecipe.Ecology.TreeTrunk = InSource.Ecology.TreeTrunk;
		OutRecipe.Ecology.TreeLeaves = InSource.Ecology.TreeLeaves;
		OutRecipe.Ecology.GrassPlant = InSource.Ecology.GrassPlant;
		CopyFrozenGenerationArray(InSource.BlockNames, OutRecipe.BlockNames);
		CopyFrozenGenerationArray(InSource.Ecology.Flowers, OutRecipe.Ecology.Flowers);
		CopyFrozenGenerationArray(InSource.Ecology.TreeSpecies, OutRecipe.Ecology.TreeSpecies);
		CopyFrozenGenerationMap(InSource.Ecology.NaturalDecorations, OutRecipe.Ecology.NaturalDecorations);
		CopyFrozenGenerationArray(InSource.Biomes, OutRecipe.Biomes);
		CopyFrozenGenerationArray(InSource.Features, OutRecipe.Features);
		CopyFrozenGenerationArray(InSource.Structures, OutRecipe.Structures);
		CopyFrozenGenerationArray(InSource.SurfaceRules, OutRecipe.SurfaceRules);
		CopyFrozenGenerationMap(InSource.BlockLookup, OutRecipe.BlockLookup);
		CopyFrozenGenerationMap(InSource.BiomeLookup, OutRecipe.BiomeLookup);
		CopyFrozenGenerationMap(InSource.FeatureLookup, OutRecipe.FeatureLookup);
		CopyFrozenGenerationMap(InSource.StructureLookup, OutRecipe.StructureLookup);
		CopyFrozenGenerationMap(InSource.SurfaceRuleLookup, OutRecipe.SurfaceRuleLookup);
	}
}

uint64 FVoxelGenerationRuntimeConfig::GetRequiredInputBytes(const FVoxelTerrainResources& InResources) const
{
	return (InputLease && InputLease->IsOwnedBy(InResources) ? InputLease->Bytes() : 0) +
		(RecipeInputLease && RecipeInputLease->IsOwnedBy(InResources) ? RecipeInputLease->Bytes() : 0);
}

int32 FVoxelGenerationRuntimeConfig::GetRequiredInputLeaseCount(const FVoxelTerrainResources& InResources) const
{
	return (InputLease && InputLease->IsOwnedBy(InResources) ? 1 : 0) +
		(RecipeInputLease && RecipeInputLease->IsOwnedBy(InResources) ? 1 : 0);
}

bool FVoxelGenerationBinding::Freeze(const FVoxelGenerationRuntimeConfig& InSource,
	TSharedRef<FVoxelTerrainResources, ESPMode::ThreadSafe> InResources,
	const FVoxelGenerationConsumerKey& InConsumer,
	TSharedPtr<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>& OutConfig,
	FString& OutError, const uint64 InRequiredInputBytes, const int32 InRequiredLeaseCount)
{
	if (!InSource.IsValid() || !InConsumer.IsValid() || InRequiredLeaseCount < 0 ||
		(InResources->GetControlConsumer().IsValid() &&
			InResources->GetControlConsumer().WorldEpoch != InConsumer.WorldEpoch))
	{
		OutError = TEXT("InvalidConsumer: frozen generation snapshot requires valid source and original consumer");
		return false;
	}
	uint64 RecipeBytes = 0;
	uint64 ConfigBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelGenerationRuntimeConfig>();
	uint64 RequiredBytes = InRequiredInputBytes;
	const int32 ExistingLeaseCount = OutConfig ? OutConfig->GetRequiredInputLeaseCount(InResources.Get()) : 0;
	if (!CountFrozenGenerationRecipe(*InSource.Recipe, RecipeBytes) ||
		!AddFrozenGenerationBytes(ConfigBytes, FrozenGenerationArrayBytes(InSource.SymbolToRuntime)) ||
		!AddFrozenGenerationBytes(ConfigBytes, FrozenGenerationArrayBytes(InSource.RuntimeToSymbol)) ||
		!AddFrozenGenerationBytes(RequiredBytes, OutConfig ? OutConfig->GetRequiredInputBytes(InResources.Get()) : 0) ||
		RecipeBytes > MAX_uint64 - ConfigBytes || ExistingLeaseCount > MAX_int32 - InRequiredLeaseCount)
	{
		OutError = TEXT("Oversized: frozen generation snapshot native capacity overflow");
		return false;
	}
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	const auto Admission = InResources->TryAcquire(InConsumer, EVoxelTerrainResourcePool::Input,
		RecipeBytes + ConfigBytes, Lease, 1, RequiredBytes, InRequiredLeaseCount + ExistingLeaseCount);
	if (Admission != EVoxelTerrainResourceAdmission::Granted)
	{
		OutError = Admission == EVoxelTerrainResourceAdmission::CapacityUnachievable ?
			TEXT("CapacityUnachievable: frozen generation snapshot and original native input") :
			Admission == EVoxelTerrainResourceAdmission::CapacityBlocked ?
			TEXT("CapacityBlocked: frozen generation snapshot native input") : TEXT("InvalidResource: frozen generation snapshot admission");
		return false;
	}
	const auto RecipeLease = Lease->Split(RecipeBytes);
	check(RecipeLease);
	auto Recipe = VoxelBoundedAllocation::MakeWorkerShared<FVoxelGenerationRecipe>(RecipeLease);
	CopyFrozenGenerationRecipe(*InSource.Recipe, *Recipe);
	auto Config = VoxelBoundedAllocation::MakeWorkerShared<FVoxelGenerationRuntimeConfig>(Lease);
	Config->InputLease = Lease;
	Config->RecipeInputLease = RecipeLease;
	Config->Recipe = MoveTemp(Recipe);
	Config->RegistryHash = InSource.RegistryHash;
	CopyFrozenGenerationArray(InSource.SymbolToRuntime, Config->SymbolToRuntime);
	CopyFrozenGenerationArray(InSource.RuntimeToSymbol, Config->RuntimeToSymbol);
	Config->Air = InSource.Air;
	Config->Stone = InSource.Stone;
	Config->Dirt = InSource.Dirt;
	Config->Grass = InSource.Grass;
	Config->Sand = InSource.Sand;
	Config->Snow = InSource.Snow;
	Config->Water = InSource.Water;
	Config->Lava = InSource.Lava;
	Config->Bedrock = InSource.Bedrock;
	Config->Road = InSource.Road;
	uint64 ActualRecipeBytes = 0;
	const uint64 ActualConfigBytes = VoxelBoundedAllocation::WorkerSharedBytes<FVoxelGenerationRuntimeConfig>() +
		VoxelGenerationAllocation::AllocatedBytes(Config->SymbolToRuntime) + VoxelGenerationAllocation::AllocatedBytes(Config->RuntimeToSymbol);
	const bool bMeasured = CountFrozenGenerationRecipe(*Config->Recipe, ActualRecipeBytes, false);
	check(bMeasured && ActualRecipeBytes <= RecipeBytes && ActualConfigBytes <= ConfigBytes);
	const bool bRecipeSettled = RecipeLease->Resize(ActualRecipeBytes);
	const bool bConfigSettled = Lease->Resize(ActualConfigBytes);
	check(bRecipeSettled && bConfigSettled);
	OutConfig = MoveTemp(Config);
	OutError.Reset();
	return true;
}

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
	if (RuntimeRecipe.AlgorithmVersion != FVoxelGenerationRecipe::CurrentAlgorithmVersion)
	{
		OutError = TEXT("Voxel generation profile requires rebaking for the current terrain algorithm");
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
