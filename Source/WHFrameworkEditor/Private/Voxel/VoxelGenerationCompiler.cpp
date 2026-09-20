#include "Voxel/VoxelGenerationCompiler.h"

#include "Algo/Sort.h"
#include "Misc/ScopedSlowTask.h"

#include "Voxel/Authoring/VoxelBiomeData.h"
#include "Voxel/Authoring/VoxelFeatureData.h"
#include "Voxel/Authoring/VoxelStructureData.h"
#include "Voxel/Authoring/VoxelSurfaceRuleData.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"

#include "Voxel/Generation/VoxelFeature.h"
#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Generation/VoxelGenerationRecipe.h"
#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"
#include "Voxel/Generation/VoxelStructure.h"

#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/Data/VoxelData.h"

namespace
{
	const FSoftObjectPath DefaultTreeTrunkPath(
		TEXT("/WHFramework/Voxel/DataAssets/Voxel/DA_Oak.DA_Oak"));
	const FSoftObjectPath DefaultTreeLeavesPath(
		TEXT("/WHFramework/Voxel/DataAssets/Voxel/DA_Oak_Leaves.DA_Oak_Leaves"));
	const FSoftObjectPath DefaultGrassPlantPath(
		TEXT("/WHFramework/Voxel/DataAssets/Voxel/DA_Tall_Grass.DA_Tall_Grass"));

	UVoxelData* LoadBuiltinEcologyVoxel(const FSoftObjectPath& InPath)
	{
		return Cast<UVoxelData>(InPath.TryLoad());
	}

	template<typename TObjectType>
	bool LoadSoftObjectRequired(
		const TSoftObjectPtr<TObjectType>& InSoftObject,
		TObjectType*& OutObject,
		const TCHAR* InLabel,
		FString& OutError)
	{
		OutObject = InSoftObject.LoadSynchronous();

		if (!OutObject)
		{
			OutError = FString::Printf(
				TEXT("Voxel generation compiler failed to load %s: %s"),
				InLabel,
				*InSoftObject.ToSoftObjectPath().ToString());

			return false;
		}

		return true;
	}

	template<typename TObjectType>
	void LoadSoftObjectsSorted(
		const TArray<TSoftObjectPtr<TObjectType>>& InSoftObjects,
		TArray<TObjectType*>& OutObjects)
	{
		OutObjects.Reset();
		OutObjects.Reserve(InSoftObjects.Num());

		for (const TSoftObjectPtr<TObjectType>& SoftObject : InSoftObjects)
		{
			if (TObjectType* Object = SoftObject.LoadSynchronous())
			{
				OutObjects.Add(Object);
			}
		}

		OutObjects.Sort(
			[](const TObjectType& InA, const TObjectType& InB)
			{
				return InA.StableId.LexicalLess(InB.StableId);
			});
	}

	FVoxelGenerationBounds GridBoxToBounds(
		const FVoxelGridBox& InBox,
		int32 InScale)
	{
		FVoxelGenerationBounds Result;

		Result.Min =
			InBox.Min *
			InScale;

		Result.Max =
			InBox.Max *
			InScale;

		return Result;
	}

	FIntVector RotateCornerQuarterTurns(
		FIntVector InValue,
		uint8 InYaw)
	{
		for (uint8 Turn = 0;
			Turn < (InYaw & 3);
			++Turn)
		{
			InValue =
				FIntVector(
					-InValue.Y,
					InValue.X,
					InValue.Z);
		}

		return InValue;
	}

	struct FCompiledCell
	{
		FIntVector Position =
			FIntVector::ZeroValue;

		uint32 Value = 0;
	};

	bool CompileCellsToRuns(
		TArray<FCompiledCell>& InOutCells,
		TArray<FVoxelStructureRun>& OutRuns,
		FString& OutError)
	{
		InOutCells.Sort(
			[](const FCompiledCell& InA, const FCompiledCell& InB)
			{
				if (InA.Position.Z != InB.Position.Z)
				{
					return InA.Position.Z < InB.Position.Z;
				}

				if (InA.Position.Y != InB.Position.Y)
				{
					return InA.Position.Y < InB.Position.Y;
				}

				return InA.Position.X < InB.Position.X;
			});

		for (int32 Index = 1;
			Index < InOutCells.Num();
			++Index)
		{
			if (InOutCells[Index - 1].Position ==
				InOutCells[Index].Position)
			{
				OutError = FString::Printf(
					TEXT("Voxel structure compiler produced duplicate cell at %s"),
					*InOutCells[Index].Position.ToString());

				return false;
			}
		}

		OutRuns.Reset();

		int32 Index = 0;

		while (Index < InOutCells.Num())
		{
			const FCompiledCell& First =
				InOutCells[Index];

			FVoxelStructureRun Run;
			Run.Start = First.Position;
			Run.Value = First.Value;
			Run.Length = 1;

			int32 NextIndex =
				Index + 1;

			while (NextIndex < InOutCells.Num())
			{
				const FCompiledCell& Previous =
					InOutCells[NextIndex - 1];

				const FCompiledCell& Current =
					InOutCells[NextIndex];

				if (Current.Value !=
						Run.Value ||
					Current.Position.Z !=
						Previous.Position.Z ||
					Current.Position.Y !=
						Previous.Position.Y ||
					Current.Position.X !=
						Previous.Position.X + 1)
				{
					break;
				}

				++Run.Length;
				++NextIndex;
			}

			OutRuns.Add(
				MoveTemp(Run));

			Index = NextIndex;
		}

		OutError.Reset();
		return true;
	}
}

bool FVoxelGenerationCompiler::Compile(
	UVoxelWorldGenerationProfile& InOutProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	FString& OutError)
{
	FVoxelGenerationRecipe Recipe;

	if (!BuildRecipe(
		InOutProfile,
		InRegistry,
		Recipe,
		OutError))
	{
		return false;
	}

	TArray<uint8> RecipeBytes;

	if (!FVoxelGenerationRecipeCodec::Encode(
		Recipe,
		RecipeBytes,
		OutError))
	{
		return false;
	}

	const uint64 RecipeHash =
		FVoxelGenerationRecipeCodec::Hash(
			RecipeBytes);

	InOutProfile.Modify();

	InOutProfile.RecipeBytes =
		MoveTemp(RecipeBytes);

	InOutProfile.RecipeHash =
		RecipeHash;

	InOutProfile.RecipeBakeVersion =
		CurrentBakeVersion;

	InOutProfile.BakedCellCentimeters =
		Recipe.CellCentimeters;

	InOutProfile.MarkPackageDirty();

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::BuildRecipe(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	FVoxelGenerationRecipe& OutRecipe,
	FString& OutError)
{
	if (InProfile.TargetCellCentimeters <= 0)
	{
		OutError = TEXT("Voxel generation profile target cell size must be positive");
		return false;
	}

	if (!InProfile.Defaults.Validate(
		OutError))
	{
		return false;
	}

	if (!ValidateBiomeFeatureCoverage(InProfile, OutError))
	{
		return false;
	}

	TArray<FName> BlockNames;

	if (!GatherBlockNames(
		InProfile,
		InRegistry,
		BlockNames,
		OutError))
	{
		return false;
	}

	FVoxelGenerationRecipe Recipe;

	Recipe.AlgorithmVersion =
		FVoxelGenerationRecipe::CurrentAlgorithmVersion;

	Recipe.CellCentimeters =
		InProfile.TargetCellCentimeters;

	Recipe.Settings =
		InProfile.Defaults;

	Recipe.BlockNames =
		MoveTemp(BlockNames);

	TMap<FName, uint16> BlockSymbols;
	BlockSymbols.Reserve(
		Recipe.BlockNames.Num());

	for (int32 Index = 1;
		Index < Recipe.BlockNames.Num();
		++Index)
	{
		if (Index > MAX_uint16)
		{
			OutError = TEXT("Voxel recipe contains more than 65535 block symbols");
			return false;
		}

		BlockSymbols.Add(
			Recipe.BlockNames[Index],
			static_cast<uint16>(Index));
	}

	if (!CompilePalette(
			InProfile,
			InRegistry,
			BlockSymbols,
			Recipe,
			OutError) ||
		!CompileBuiltinEcology(
			InProfile,
			InRegistry,
			BlockSymbols,
			Recipe,
			OutError) ||
		!CompileSurfaceRules(
			InProfile,
			InRegistry,
			BlockSymbols,
			Recipe,
			OutError) ||
		!CompileFeatures(
			InProfile,
			InRegistry,
			BlockSymbols,
			Recipe,
			OutError) ||
		!CompileStructures(
			InProfile,
			InRegistry,
			BlockSymbols,
			Recipe,
			OutError) ||
		!CompileBiomes(
			InProfile,
			Recipe,
			OutError))
	{
		return false;
	}

	if (!Recipe.BuildLookups(
		OutError))
	{
		return false;
	}

	/**
	 * BuildRecipe 阶段 RecipeHash 还没有写入。
	 * Compile() canonical encode 后才得到最终 Hash。
	 */
	Recipe.RecipeHash = 0;

	OutRecipe =
		MoveTemp(Recipe);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::GatherBlockNames(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	TArray<FName>& OutBlockNames,
	FString& OutError)
{
	TSet<FName> Names;

	auto AddRequired =
		[&Names, &InRegistry, &OutError](
			const TSoftObjectPtr<UVoxelData>& InSoftVoxel,
			const TCHAR* InLabel) -> bool
		{
			UVoxelData* Voxel =
				InSoftVoxel.LoadSynchronous();

			if (!Voxel)
			{
				OutError = FString::Printf(
					TEXT("Voxel generation profile is missing required block: %s"),
					InLabel);

				return false;
			}

			return FVoxelGenerationCompiler::AddReferencedVoxel(
				Voxel,
				InRegistry,
				Names,
				OutError);
		};

	if (!AddRequired(InProfile.Stone, TEXT("Stone")) ||
		!AddRequired(InProfile.Dirt, TEXT("Dirt")) ||
		!AddRequired(InProfile.Grass, TEXT("Grass")) ||
		!AddRequired(InProfile.Sand, TEXT("Sand")) ||
		!AddRequired(InProfile.Snow, TEXT("Snow")) ||
		!AddRequired(InProfile.Water, TEXT("Water")) ||
		!AddRequired(InProfile.Lava, TEXT("Lava")) ||
		!AddRequired(InProfile.Bedrock, TEXT("Bedrock")))
	{
		return false;
	}

	if (UVoxelData* Road =
		InProfile.Road.LoadSynchronous())
	{
		if (!AddReferencedVoxel(
			Road,
			InRegistry,
			Names,
			OutError))
		{
			return false;
		}
	}

	if (!GatherBuiltinEcologyBlockNames(
		InProfile,
		InRegistry,
		Names,
		OutError))
	{
		return false;
	}

	for (const TSoftObjectPtr<UVoxelSurfaceRuleSet>& SoftRuleSet :
		InProfile.SurfaceRuleSets)
	{
		UVoxelSurfaceRuleSet* RuleSet =
			SoftRuleSet.LoadSynchronous();

		if (!RuleSet)
		{
			OutError = FString::Printf(
				TEXT("Voxel generation compiler failed to load surface rule set: %s"),
				*SoftRuleSet.ToSoftObjectPath().ToString());

			return false;
		}

		for (const FVoxelSurfaceRuleData& Rule :
			RuleSet->Rules)
		{
			UVoxelData* Block =
				Rule.Block.LoadSynchronous();

			if (!Block)
			{
				OutError = FString::Printf(
					TEXT("Surface rule set %s contains an invalid block"),
					*RuleSet->StableId.ToString());

				return false;
			}

			if (!AddReferencedVoxel(
				Block,
				InRegistry,
				Names,
				OutError))
			{
				return false;
			}
		}
	}

	for (const TSoftObjectPtr<UVoxelStructureData>& SoftStructure :
		InProfile.Structures)
	{
		UVoxelStructureData* Structure =
			SoftStructure.LoadSynchronous();

		if (!Structure)
		{
			OutError = FString::Printf(
				TEXT("Voxel generation compiler failed to load structure: %s"),
				*SoftStructure.ToSoftObjectPath().ToString());

			return false;
		}

		UVoxelPrefabData* Prefab =
			Structure->SourcePrefab.LoadSynchronous();

		if (Prefab)
		{
			for (const FVoxelPrefabCell& Cell :
				Prefab->Data.Cells)
			{
				FName BlockName;

				if (!ResolveBlockName(
					Cell.Item.VoxelAssetID,
					InRegistry,
					BlockName,
					OutError))
				{
					return false;
				}

				Names.Add(BlockName);
			}
		}

		for (const FVoxelStaticBlockMapping& Mapping :
			Structure->StaticMappings)
		{
			if (UVoxelData* Replacement =
				Mapping.SolidReplacement.LoadSynchronous())
			{
				if (!AddReferencedVoxel(
					Replacement,
					InRegistry,
					Names,
					OutError))
				{
					return false;
				}
			}
		}
	}

	if (!GatherFeatureBlockNames(InProfile, InRegistry, Names, OutError))
	{
		return false;
	}

	TArray<FName> SortedNames =
		Names.Array();

	SortedNames.Sort(
		FNameLexicalLess());

	OutBlockNames.Reset();
	OutBlockNames.Reserve(
		SortedNames.Num() + 1);

	/**
	 * Symbol 0 始终为空名，代表 Air。
	 */
	OutBlockNames.Add(NAME_None);
	OutBlockNames.Append(SortedNames);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::GatherBuiltinEcologyBlockNames(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	TSet<FName>& InOutNames,
	FString& OutError)
{
	const FVoxelEcologyGenerationSettings& Ecology = InProfile.Defaults.Ecology;
	if (Ecology.Tree.bEnabled)
	{
		UVoxelData* Trunk = LoadBuiltinEcologyVoxel(DefaultTreeTrunkPath);
		UVoxelData* Leaves = LoadBuiltinEcologyVoxel(DefaultTreeLeavesPath);
		if (!Trunk || !Leaves)
		{
			OutError = TEXT("WHFramework default tree ecology assets are missing");
			return false;
		}
		if (!AddReferencedVoxel(Trunk, InRegistry, InOutNames, OutError) ||
			!AddReferencedVoxel(Leaves, InRegistry, InOutNames, OutError))
		{
			return false;
		}
	}
	if (Ecology.Grass.bEnabled)
	{
		UVoxelData* GrassPlant = LoadBuiltinEcologyVoxel(DefaultGrassPlantPath);
		if (!GrassPlant)
		{
			OutError = TEXT("WHFramework default grass ecology asset is missing");
			return false;
		}
		if (!AddReferencedVoxel(GrassPlant, InRegistry, InOutNames, OutError))
		{
			return false;
		}
	}
	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::GatherFeatureBlockNames(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	TSet<FName>& InOutNames,
	FString& OutError)
{
	for (const TSoftObjectPtr<UVoxelFeatureData>& SoftFeature : InProfile.Features)
	{
		UVoxelFeatureData* Feature = SoftFeature.LoadSynchronous();
		if (!Feature)
		{
			OutError = FString::Printf(TEXT("Voxel generation compiler failed to load feature: %s"), *SoftFeature.ToSoftObjectPath().ToString());
			return false;
		}

		TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> Algorithm =
			FVoxelFeatureAlgorithmRegistry::Get().Find(Feature->AlgorithmId);
		if (!Algorithm)
		{
			OutError = FString::Printf(TEXT("Voxel feature algorithm is not registered: %s"), *Feature->AlgorithmId.ToString());
			return false;
		}

		TArray<FPrimaryAssetId> ReferencedBlocks;
		if (!Algorithm->GatherReferencedBlocks(Feature->Configuration, ReferencedBlocks, OutError))
		{
			OutError = FString::Printf(TEXT("Failed to gather feature %s block dependencies: %s"), *Feature->StableId.ToString(), *OutError);
			return false;
		}

		for (const FPrimaryAssetId& AssetId : ReferencedBlocks)
		{
			FName BlockName;
			if (!ResolveBlockName(AssetId, InRegistry, BlockName, OutError))
			{
				return false;
			}
			InOutNames.Add(BlockName);
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompilePalette(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	FVoxelGenerationRecipe& InOutRecipe,
	FString& OutError)
{
	InOutRecipe.Palette.Air = 0;

	auto ResolveRequired =
		[&InRegistry, &InBlockSymbols, &OutError](
			const TSoftObjectPtr<UVoxelData>& InSoftVoxel,
			uint16& OutSymbol) -> bool
		{
			UVoxelData* Voxel =
				InSoftVoxel.LoadSynchronous();

			if (!Voxel)
			{
				OutError = TEXT("Voxel generation palette contains an unloaded required block");
				return false;
			}

			return FVoxelGenerationCompiler::ResolveBlockSymbol(
				Voxel,
				InRegistry,
				InBlockSymbols,
				OutSymbol,
				OutError);
		};

	if (!ResolveRequired(InProfile.Stone, InOutRecipe.Palette.Stone) ||
		!ResolveRequired(InProfile.Dirt, InOutRecipe.Palette.Dirt) ||
		!ResolveRequired(InProfile.Grass, InOutRecipe.Palette.Grass) ||
		!ResolveRequired(InProfile.Sand, InOutRecipe.Palette.Sand) ||
		!ResolveRequired(InProfile.Snow, InOutRecipe.Palette.Snow) ||
		!ResolveRequired(InProfile.Water, InOutRecipe.Palette.Water) ||
		!ResolveRequired(InProfile.Lava, InOutRecipe.Palette.Lava) ||
		!ResolveRequired(InProfile.Bedrock, InOutRecipe.Palette.Bedrock))
	{
		return false;
	}

	InOutRecipe.Palette.Road =
		MAX_uint16;

	if (UVoxelData* Road =
		InProfile.Road.LoadSynchronous())
	{
		if (!ResolveBlockSymbol(
			Road,
			InRegistry,
			InBlockSymbols,
			InOutRecipe.Palette.Road,
			OutError))
		{
			return false;
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompileBuiltinEcology(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	FVoxelGenerationRecipe& InOutRecipe,
	FString& OutError)
{
	InOutRecipe.Ecology = FVoxelEcologyRuntimePalette();
	const FVoxelEcologyGenerationSettings& Ecology = InProfile.Defaults.Ecology;
	if (Ecology.Tree.bEnabled)
	{
		UVoxelData* Trunk = LoadBuiltinEcologyVoxel(DefaultTreeTrunkPath);
		UVoxelData* Leaves = LoadBuiltinEcologyVoxel(DefaultTreeLeavesPath);
		if (!Trunk || !Leaves)
		{
			OutError = TEXT("WHFramework default tree ecology assets are missing");
			return false;
		}
		if (!ResolveBlockSymbol(Trunk, InRegistry, InBlockSymbols, InOutRecipe.Ecology.TreeTrunk, OutError) ||
			!ResolveBlockSymbol(Leaves, InRegistry, InBlockSymbols, InOutRecipe.Ecology.TreeLeaves, OutError))
		{
			return false;
		}
	}
	if (Ecology.Grass.bEnabled)
	{
		UVoxelData* GrassPlant = LoadBuiltinEcologyVoxel(DefaultGrassPlantPath);
		if (!GrassPlant)
		{
			OutError = TEXT("WHFramework default grass ecology asset is missing");
			return false;
		}
		if (!ResolveBlockSymbol(GrassPlant, InRegistry, InBlockSymbols, InOutRecipe.Ecology.GrassPlant, OutError))
		{
			return false;
		}
	}
	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompileSurfaceRules(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	FVoxelGenerationRecipe& InOutRecipe,
	FString& OutError)
{
	TArray<UVoxelSurfaceRuleSet*> RuleSets;
	LoadSoftObjectsSorted(
		InProfile.SurfaceRuleSets,
		RuleSets);

	InOutRecipe.SurfaceRules.Reset();
	InOutRecipe.SurfaceRules.Reserve(
		RuleSets.Num());

	for (UVoxelSurfaceRuleSet* RuleSet :
		RuleSets)
	{
		if (!RuleSet ||
			RuleSet->StableId.IsNone())
		{
			OutError = TEXT("Voxel surface rule set contains an invalid StableId");
			return false;
		}

		FVoxelSurfaceRuntimeRuleSet RuntimeRuleSet;
		RuntimeRuleSet.StableId =
			RuleSet->StableId;

		RuntimeRuleSet.Rules.Reserve(
			RuleSet->Rules.Num());

		for (const FVoxelSurfaceRuleData& Rule :
			RuleSet->Rules)
		{
			UVoxelData* Block =
				Rule.Block.LoadSynchronous();

			if (!Block)
			{
				OutError = FString::Printf(
					TEXT("Surface rule set %s contains an unloaded block"),
					*RuleSet->StableId.ToString());

				return false;
			}

			FVoxelSurfaceRuntimeRule RuntimeRule;

			RuntimeRule.Height =
				Rule.Height;

			RuntimeRule.Slope =
				Rule.Slope;

			RuntimeRule.Temperature =
				Rule.Temperature;

			RuntimeRule.Moisture =
				Rule.Moisture;

			RuntimeRule.MinDepth =
				Rule.MinDepth;

			RuntimeRule.MaxDepth =
				Rule.MaxDepth;

			if (!ResolveBlockSymbol(
				Block,
				InRegistry,
				InBlockSymbols,
				RuntimeRule.BlockSymbol,
				OutError))
			{
				return false;
			}

			RuntimeRule.bRiverOnly =
				Rule.bRiverOnly;

			RuntimeRule.bLakeOnly =
				Rule.bLakeOnly;

			RuntimeRule.bOceanOnly =
				Rule.bOceanOnly;

			RuntimeRule.bCoastOnly =
				Rule.bCoastOnly;

			RuntimeRuleSet.Rules.Add(
				MoveTemp(RuntimeRule));
		}

		InOutRecipe.SurfaceRules.Add(
			MoveTemp(RuntimeRuleSet));
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompileFeatures(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	FVoxelGenerationRecipe& InOutRecipe,
	FString& OutError)
{
	TArray<UVoxelFeatureData*> Features;
	LoadSoftObjectsSorted(
		InProfile.Features,
		Features);

	InOutRecipe.Features.Reset();
	InOutRecipe.Features.Reserve(
		Features.Num());

	const FVoxelFeatureBakeContext BakeContext { &InRegistry, &InBlockSymbols };

	for (UVoxelFeatureData* Feature :
		Features)
	{
		if (!Feature)
		{
			continue;
		}

		if (Feature->StableId.IsNone() ||
			Feature->AlgorithmId.IsNone())
		{
			OutError = TEXT("Voxel feature contains an invalid StableId or AlgorithmId");
			return false;
		}

		TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> Algorithm =
			FVoxelFeatureAlgorithmRegistry::Get().Find(
				Feature->AlgorithmId);

		if (!Algorithm)
		{
			OutError = FString::Printf(
				TEXT("Voxel feature algorithm is not registered: %s"),
				*Feature->AlgorithmId.ToString());

			return false;
		}

		FVoxelFeatureRuntimeDefinition RuntimeFeature;

		RuntimeFeature.StableId =
			Feature->StableId;

		RuntimeFeature.StableHash =
			VoxelGeneration::HashBytes(
				MakeArrayView(
					reinterpret_cast<const uint8*>(
						TCHAR_TO_UTF8(
							*Feature->StableId.ToString())),
					FTCHARToUTF8(
						*Feature->StableId.ToString()).Length()));

		RuntimeFeature.AlgorithmId =
			Feature->AlgorithmId;

		RuntimeFeature.AlgorithmVersion =
			Algorithm->GetVersion();

		RuntimeFeature.Stage =
			Feature->Stage;

		RuntimeFeature.Placement =
			Feature->Placement;

		if (!Algorithm->BakeConfiguration(
			BakeContext,
			Feature->Configuration,
			RuntimeFeature.ConfigBytes,
			OutError))
		{
			OutError = FString::Printf(
				TEXT("Failed to bake feature %s: %s"),
				*Feature->StableId.ToString(),
				*OutError);

			return false;
		}

		InOutRecipe.Features.Add(
			MoveTemp(RuntimeFeature));
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::ValidateBiomeFeatureCoverage(
	const UVoxelWorldGenerationProfile& InProfile,
	FString& OutError)
{
	if (InProfile.Biomes.IsEmpty())
	{
		OutError =
			TEXT(
				"Voxel generation profile must contain at least one biome");

		return false;
	}

	TSet<FName> ReferencedFeatures;

	bool bHasFallbackBiome =
		false;

	for (const TSoftObjectPtr<UVoxelBiomeData>& SoftBiome :
		InProfile.Biomes)
	{
		UVoxelBiomeData* Biome =
			SoftBiome.
				LoadSynchronous();

		if (!Biome)
		{
			OutError =
				TEXT(
					"Voxel generation profile contains an unloaded biome");

			return false;
		}

		const bool bFullTemperature =
			Biome->Temperature.Min ==
				MIN_int32 &&
			Biome->Temperature.Max ==
				MAX_int32;

		const bool bFullMoisture =
			Biome->Moisture.Min ==
				MIN_int32 &&
			Biome->Moisture.Max ==
				MAX_int32;

		const bool bFullContinentalness =
			Biome->Continentalness.Min ==
				MIN_int32 &&
			Biome->Continentalness.Max ==
				MAX_int32;

		const bool bFullErosion =
			Biome->Erosion.Min ==
				MIN_int32 &&
			Biome->Erosion.Max ==
				MAX_int32;

		const bool bFullHeight =
			Biome->Height.Min ==
				MIN_int32 &&
			Biome->Height.Max ==
				MAX_int32;

		const bool bFullSlope =
			Biome->Slope.Min ==
				MIN_int32 &&
			Biome->Slope.Max ==
				MAX_int32;

		bHasFallbackBiome |=
			bFullTemperature &&
			bFullMoisture &&
			bFullContinentalness &&
			bFullErosion &&
			bFullHeight &&
			bFullSlope;

		for (const TSoftObjectPtr<UVoxelFeatureData>& SoftFeature :
			Biome->Features)
		{
			if (UVoxelFeatureData* Feature =
				SoftFeature.
					LoadSynchronous())
			{
				ReferencedFeatures.Add(
					Feature->
						StableId);
			}
		}
	}

	if (!bHasFallbackBiome)
	{
		OutError =
			TEXT(
				"Voxel generation profile has no full-range fallback biome");

		return false;
	}

	for (const TSoftObjectPtr<UVoxelFeatureData>& SoftFeature :
		InProfile.Features)
	{
		UVoxelFeatureData* Feature =
			SoftFeature.
				LoadSynchronous();

		if (Feature &&
			!ReferencedFeatures.Contains(
				Feature->StableId))
		{
			OutError =
				FString::Printf(
					TEXT(
						"Voxel feature %s is not referenced by any biome"),
					*Feature->
						StableId.
						ToString());

			return false;
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompileStructures(
	const UVoxelWorldGenerationProfile& InProfile,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	FVoxelGenerationRecipe& InOutRecipe,
	FString& OutError)
{
	TArray<UVoxelStructureData*> Structures;
	LoadSoftObjectsSorted(
		InProfile.Structures,
		Structures);

	InOutRecipe.Structures.Reset();
	InOutRecipe.Structures.Reserve(
		Structures.Num());

	for (UVoxelStructureData* Structure :
		Structures)
	{
		if (!Structure)
		{
			continue;
		}

		FVoxelStructureRuntimeDefinition RuntimeStructure;

		RuntimeStructure.StableId =
			Structure->StableId;

		RuntimeStructure.StableHash =
			VoxelGeneration::MakeStableId(
				0,
				FIntVector::ZeroValue,
				GetTypeHash(
					Structure->StableId),
				0).High;

		RuntimeStructure.Stage =
			Structure->Stage;

		RuntimeStructure.Placement =
			Structure->Placement;

		RuntimeStructure.Adaptation =
			Structure->Adaptation;

		RuntimeStructure.LayoutAlgorithmId =
			Structure->LayoutAlgorithmId;

		if (!Structure->LayoutAlgorithmId.IsNone())
		{
			TSharedPtr<
				const IVoxelStructureLayoutAlgorithm,
				ESPMode::ThreadSafe> LayoutAlgorithm =
				FVoxelStructureLayoutRegistry::Get().Find(
					Structure->LayoutAlgorithmId);

			if (!LayoutAlgorithm)
			{
				OutError = FString::Printf(
					TEXT("Voxel structure layout algorithm is not registered: %s"),
					*Structure->LayoutAlgorithmId.ToString());

				return false;
			}

			RuntimeStructure.LayoutAlgorithmVersion =
				LayoutAlgorithm->GetVersion();
		}

		if (Structure->SourcePrefab.IsValid() ||
			!Structure->SourcePrefab.IsNull())
		{
			FVoxelStructurePieceTemplate Piece;

			if (!CompileStructurePiece(
				*Structure,
				InRegistry,
				InBlockSymbols,
				InProfile.TargetCellCentimeters,
				Piece,
				OutError))
			{
				return false;
			}

			RuntimeStructure.Pieces.Add(
				MoveTemp(Piece));
		}

		InOutRecipe.Structures.Add(
			MoveTemp(RuntimeStructure));
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompileBiomes(
	const UVoxelWorldGenerationProfile& InProfile,
	FVoxelGenerationRecipe& InOutRecipe,
	FString& OutError)
{
	TArray<UVoxelBiomeData*> Biomes;
	LoadSoftObjectsSorted(
		InProfile.Biomes,
		Biomes);

	TMap<FName, int32> SurfaceRuleIndices;
	TMap<FName, int32> FeatureIndices;
	TMap<FName, int32> StructureIndices;

	for (int32 Index = 0;
		Index < InOutRecipe.SurfaceRules.Num();
		++Index)
	{
		SurfaceRuleIndices.Add(
			InOutRecipe.SurfaceRules[Index].StableId,
			Index);
	}

	for (int32 Index = 0;
		Index < InOutRecipe.Features.Num();
		++Index)
	{
		FeatureIndices.Add(
			InOutRecipe.Features[Index].StableId,
			Index);
	}

	for (int32 Index = 0;
		Index < InOutRecipe.Structures.Num();
		++Index)
	{
		StructureIndices.Add(
			InOutRecipe.Structures[Index].StableId,
			Index);
	}

	InOutRecipe.Biomes.Reset();
	InOutRecipe.Biomes.Reserve(
		Biomes.Num());

	for (UVoxelBiomeData* Biome :
		Biomes)
	{
		if (!Biome ||
			Biome->StableId.IsNone())
		{
			OutError = TEXT("Voxel generation profile contains an invalid biome");
			return false;
		}

		FVoxelBiomeRuntimeDefinition RuntimeBiome;

		RuntimeBiome.StableId =
			Biome->StableId;

		const FString BiomeString =
			Biome->StableId.ToString();

		FTCHARToUTF8 BiomeUtf8(
			*BiomeString);

		RuntimeBiome.StableHash =
			VoxelGeneration::HashBytes(
				MakeArrayView(
					reinterpret_cast<const uint8*>(
						BiomeUtf8.Get()),
					BiomeUtf8.Length()));

		RuntimeBiome.Priority =
			Biome->Priority;

		RuntimeBiome.Temperature =
			Biome->Temperature;

		RuntimeBiome.Moisture =
			Biome->Moisture;

		RuntimeBiome.Continentalness =
			Biome->Continentalness;

		RuntimeBiome.Erosion =
			Biome->Erosion;

		RuntimeBiome.Height =
			Biome->Height;

		RuntimeBiome.Slope =
			Biome->Slope;

		RuntimeBiome.SurfaceRuleIndex =
			INDEX_NONE;

		if (UVoxelSurfaceRuleSet* SurfaceRules =
			Biome->SurfaceRules.LoadSynchronous())
		{
			const int32* Index =
				SurfaceRuleIndices.Find(
					SurfaceRules->StableId);

			if (!Index)
			{
				OutError = FString::Printf(
					TEXT("Biome %s references surface rule set %s that is not in the profile"),
					*Biome->StableId.ToString(),
					*SurfaceRules->StableId.ToString());

				return false;
			}

			RuntimeBiome.SurfaceRuleIndex =
				*Index;
		}

		for (const TSoftObjectPtr<UVoxelFeatureData>& SoftFeature :
			Biome->Features)
		{
			UVoxelFeatureData* Feature =
				SoftFeature.LoadSynchronous();

			if (!Feature)
			{
				OutError = FString::Printf(
					TEXT("Biome %s contains an invalid feature reference"),
					*Biome->StableId.ToString());

				return false;
			}

			const int32* Index =
				FeatureIndices.Find(
					Feature->StableId);

			if (!Index)
			{
				OutError = FString::Printf(
					TEXT("Biome %s references feature %s that is not in the profile"),
					*Biome->StableId.ToString(),
					*Feature->StableId.ToString());

				return false;
			}

			RuntimeBiome.FeatureIndices.AddUnique(
				*Index);
		}

		for (const TSoftObjectPtr<UVoxelStructureData>& SoftStructure :
			Biome->Structures)
		{
			UVoxelStructureData* Structure =
				SoftStructure.LoadSynchronous();

			if (!Structure)
			{
				OutError = FString::Printf(
					TEXT("Biome %s contains an invalid structure reference"),
					*Biome->StableId.ToString());

				return false;
			}

			const int32* Index =
				StructureIndices.Find(
					Structure->StableId);

			if (!Index)
			{
				OutError = FString::Printf(
					TEXT("Biome %s references structure %s that is not in the profile"),
					*Biome->StableId.ToString(),
					*Structure->StableId.ToString());

				return false;
			}

			RuntimeBiome.StructureIndices.AddUnique(
				*Index);
		}

		RuntimeBiome.FeatureIndices.Sort();
		RuntimeBiome.StructureIndices.Sort();

		InOutRecipe.Biomes.Add(
			MoveTemp(RuntimeBiome));
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::CompileStructurePiece(
	const UVoxelStructureData& InStructure,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	int32 InTargetCellCentimeters,
	FVoxelStructurePieceTemplate& OutPiece,
	FString& OutError)
{
	UVoxelPrefabData* Prefab =
		InStructure.SourcePrefab.LoadSynchronous();

	if (!Prefab)
	{
		OutError = FString::Printf(
			TEXT("Structure %s has no source prefab"),
			*InStructure.StableId.ToString());

		return false;
	}

	if (InStructure.SourceCellCentimeters <= 0 ||
		InTargetCellCentimeters <= 0 ||
		InStructure.SourceCellCentimeters %
			InTargetCellCentimeters != 0)
	{
		OutError = FString::Printf(
			TEXT("Structure %s source cell size must be an integer multiple of target cell size"),
			*InStructure.StableId.ToString());

		return false;
	}

	const int32 Scale =
		InStructure.SourceCellCentimeters /
		InTargetCellCentimeters;

	if (Scale <= 0 || Scale > 16)
	{
		OutError = FString::Printf(
			TEXT("Structure %s source-to-target scale is outside [1,16]"),
			*InStructure.StableId.ToString());

		return false;
	}

	TMap<FPrimaryAssetId, FPrimaryAssetId> ReplacementMap;

	for (const FVoxelStaticBlockMapping& Mapping :
		InStructure.StaticMappings)
	{
		UVoxelData* Source =
			Mapping.Source.LoadSynchronous();

		UVoxelData* Replacement =
			Mapping.SolidReplacement.LoadSynchronous();

		if (!Source || !Replacement)
		{
			OutError = FString::Printf(
				TEXT("Structure %s contains an invalid static block mapping"),
				*InStructure.StableId.ToString());

			return false;
		}

		ReplacementMap.Add(
			Source->GetPrimaryAssetId(),
			Replacement->GetPrimaryAssetId());
	}

	TArray<FCompiledCell> Cells;

	for (const FVoxelPrefabCell& SourceCell :
		Prefab->Data.Cells)
	{
		FPrimaryAssetId AssetId =
			SourceCell.Item.VoxelAssetID;

		if (const FPrimaryAssetId* Replacement =
			ReplacementMap.Find(AssetId))
		{
			AssetId = *Replacement;
		}

		FName BlockName;

		if (!ResolveBlockName(
			AssetId,
			InRegistry,
			BlockName,
			OutError))
		{
			return false;
		}

		const uint16* Symbol =
			InBlockSymbols.Find(
				BlockName);

		if (!Symbol)
		{
			OutError = FString::Printf(
				TEXT("Structure %s uses block %s that is missing from recipe block symbols"),
				*InStructure.StableId.ToString(),
				*BlockName.ToString());

			return false;
		}

		const uint32 PackedValue =
			static_cast<uint32>(*Symbol) |
			(
				static_cast<uint32>(
					static_cast<uint16>(
						SourceCell.Item.State)) <<
				16
			);

		const FIntVector Base =
			SourceCell.Offset *
			Scale;

		for (int32 LocalZ = 0;
			LocalZ < Scale;
			++LocalZ)
		{
			for (int32 LocalY = 0;
				LocalY < Scale;
				++LocalY)
			{
				for (int32 LocalX = 0;
					LocalX < Scale;
					++LocalX)
				{
					FCompiledCell Cell;

					Cell.Position =
						Base +
						FIntVector(
							LocalX,
							LocalY,
							LocalZ);

					Cell.Value =
						PackedValue;

					Cells.Add(
						MoveTemp(Cell));
				}
			}
		}
	}

	FVoxelStructurePieceTemplate Piece;

	Piece.StableId =
		InStructure.StableId;

	Piece.SourceCellCentimeters =
		InStructure.SourceCellCentimeters;

	Piece.GroundZ =
		InStructure.SourceGroundCellZ *
		Scale;

	Piece.Bounds =
		GridBoxToBounds(
			InStructure.SourceBounds,
			Scale);

	Piece.Entrance =
		InStructure.SourceEntranceCorner *
		Scale;

	for (const FVoxelGridBox& ClearVolume :
		InStructure.ClearVolumes)
	{
		Piece.ClearVolumes.Add(
			GridBoxToBounds(
				ClearVolume,
				Scale));
	}

	Piece.Connectors =
		InStructure.Connectors;

	for (FVoxelStructureConnectorData& Connector :
		Piece.Connectors)
	{
		Connector.Position *=
			Scale;
	}

	for (const FVoxelStructureDetailSocket& Detail :
		InStructure.Details)
	{
		UVoxelDetailData* DetailData =
			Detail.Detail.LoadSynchronous();

		if (!DetailData)
		{
			OutError = FString::Printf(
				TEXT("Structure %s contains an invalid detail socket"),
				*InStructure.StableId.ToString());

			return false;
		}

		FVoxelStructureDetailRuntimeSocket RuntimeDetail;

		RuntimeDetail.DetailId =
			DetailData->StableKey;

		RuntimeDetail.SourceCorner =
			Detail.SourceCorner *
			Scale;

		RuntimeDetail.Yaw =
			Detail.Yaw;

		Piece.Details.Add(
			MoveTemp(RuntimeDetail));
	}

	if (!CompileCellsToRuns(
		Cells,
		Piece.Writes,
		OutError))
	{
		return false;
	}

	if (!Piece.Validate(
		InBlockSymbols.Num() + 1,
		OutError))
	{
		return false;
	}

	OutPiece =
		MoveTemp(Piece);

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::ResolveBlockName(
	const UVoxelData* InVoxelData,
	const FVoxelRegistrySnapshot& InRegistry,
	FName& OutBlockName,
	FString& OutError)
{
	if (!InVoxelData)
	{
		OutError = TEXT("Voxel generation compiler received a null UVoxelData");
		return false;
	}

	return ResolveBlockName(
		InVoxelData->GetPrimaryAssetId(),
		InRegistry,
		OutBlockName,
		OutError);
}

bool FVoxelGenerationCompiler::ResolveBlockName(
	const FPrimaryAssetId& InAssetId,
	const FVoxelRegistrySnapshot& InRegistry,
	FName& OutBlockName,
	FString& OutError)
{
	if (!InAssetId.IsValid())
	{
		OutError = TEXT("Voxel generation compiler received an invalid voxel asset id");
		return false;
	}

	const FVoxelRuntimeDefinition* Definition =
		InRegistry.Find(InAssetId);

	if (!Definition)
	{
		OutError = FString::Printf(
			TEXT("Voxel asset is not present in runtime registry: %s"),
			*InAssetId.ToString());

		return false;
	}

	if (Definition->BlockName.IsNone())
	{
		OutError = FString::Printf(
			TEXT("Voxel registry definition has no stable BlockName: %s"),
			*InAssetId.ToString());

		return false;
	}

	OutBlockName =
		Definition->BlockName;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::ResolveBlockSymbol(
	const UVoxelData* InVoxelData,
	const FVoxelRegistrySnapshot& InRegistry,
	const TMap<FName, uint16>& InBlockSymbols,
	uint16& OutSymbol,
	FString& OutError)
{
	FName BlockName;

	if (!ResolveBlockName(
		InVoxelData,
		InRegistry,
		BlockName,
		OutError))
	{
		return false;
	}

	const uint16* Symbol =
		InBlockSymbols.Find(
			BlockName);

	if (!Symbol)
	{
		OutError = FString::Printf(
			TEXT("Voxel block %s is not present in recipe block symbols"),
			*BlockName.ToString());

		return false;
	}

	OutSymbol = *Symbol;

	OutError.Reset();
	return true;
}

bool FVoxelGenerationCompiler::AddReferencedVoxel(
	const UVoxelData* InVoxelData,
	const FVoxelRegistrySnapshot& InRegistry,
	TSet<FName>& InOutNames,
	FString& OutError)
{
	FName BlockName;

	if (!ResolveBlockName(
		InVoxelData,
		InRegistry,
		BlockName,
		OutError))
	{
		return false;
	}

	InOutNames.Add(
		BlockName);

	OutError.Reset();
	return true;
}
