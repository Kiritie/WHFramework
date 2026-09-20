#include "Voxel/Generation/VoxelGenerationRecipe.h"

namespace
{
	template<typename T>
	bool VoxelGenerationValidateUniqueNames(const TArray<T>& InValues, const TCHAR* InLabel, FString& OutError)
	{
		TSet<FName> Names;
		Names.Reserve(InValues.Num());

		for (const T& Value : InValues)
		{
			if (Value.StableId.IsNone())
			{
				OutError = FString::Printf(TEXT("%s contains an empty StableId"), InLabel);
				return false;
			}

			if (Names.Contains(Value.StableId))
			{
				OutError = FString::Printf(TEXT("%s contains duplicate StableId: %s"), InLabel, *Value.StableId.ToString());
				return false;
			}

			Names.Add(Value.StableId);
		}

		return true;
	}

	bool VoxelGenerationValidateIndices(TConstArrayView<int32> InIndices, int32 InCount, const TCHAR* InLabel, FString& OutError)
	{
		for (const int32 Index : InIndices)
		{
			if (Index < 0 || Index >= InCount)
			{
				OutError = FString::Printf(TEXT("%s contains an out-of-range index: %d"), InLabel, Index);
				return false;
			}
		}

		return true;
	}
}

bool FVoxelFeaturePlacement::Validate(FString& OutError) const
{
	if (Spacing <= 0)
	{
		OutError = TEXT("Voxel feature spacing must be positive");
		return false;
	}
	if (ChancePermille < 0 || ChancePermille > 1000)
	{
		OutError = TEXT("Voxel feature chance must be in [0, 1000]");
		return false;
	}
	if (MinZ > MaxZ)
	{
		OutError = TEXT("Voxel feature MinZ exceeds MaxZ");
		return false;
	}
	if (MaxSlopePermille < 0)
	{
		OutError = TEXT("Voxel feature maximum slope cannot be negative");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelStructurePlacement::Validate(FString& OutError) const
{
	if (Spacing <= 0)
	{
		OutError = TEXT("Voxel structure spacing must be positive");
		return false;
	}
	if (Separation < 0 || Separation * 2 >= Spacing)
	{
		OutError = TEXT("Voxel structure separation must be non-negative and smaller than half spacing");
		return false;
	}
	if (ChancePermille < 0 || ChancePermille > 1000)
	{
		OutError = TEXT("Voxel structure chance must be in [0, 1000]");
		return false;
	}
	if (MinZ > MaxZ)
	{
		OutError = TEXT("Voxel structure MinZ exceeds MaxZ");
		return false;
	}
	if (MaxSlopePermille < 0)
	{
		OutError = TEXT("Voxel structure maximum slope cannot be negative");
		return false;
	}
	if (MaxTerrainAdjustment < 0)
	{
		OutError = TEXT("Voxel structure terrain adjustment cannot be negative");
		return false;
	}
	if (BlendRadius < 0)
	{
		OutError = TEXT("Voxel structure blend radius cannot be negative");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationPalette::Validate(int32 InBlockCount, FString& OutError) const
{
	if (Air != 0)
	{
		OutError = TEXT("Voxel recipe Air symbol must be zero");
		return false;
	}

	const auto IsRequiredValid = [InBlockCount](uint16 InSymbol)
	{
		return InSymbol != MAX_uint16 && static_cast<int32>(InSymbol) < InBlockCount;
	};

	if (!IsRequiredValid(Stone) || !IsRequiredValid(Dirt) || !IsRequiredValid(Grass) || !IsRequiredValid(Sand) ||
		!IsRequiredValid(Snow) || !IsRequiredValid(Water) || !IsRequiredValid(Lava) || !IsRequiredValid(Bedrock))
	{
		OutError = TEXT("Voxel recipe contains an invalid required palette symbol");
		return false;
	}

	if (Road != MAX_uint16 && static_cast<int32>(Road) >= InBlockCount)
	{
		OutError = TEXT("Voxel recipe Road symbol is invalid");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelEcologyRuntimePalette::Validate(
	const int32 InBlockCount,
	const FVoxelEcologyGenerationSettings& InSettings,
	FString& OutError) const
{
	const auto IsValid = [InBlockCount](const uint16 InSymbol)
	{
		return InSymbol != MAX_uint16 && static_cast<int32>(InSymbol) < InBlockCount;
	};

	if (InSettings.Tree.bEnabled && (!IsValid(TreeTrunk) || !IsValid(TreeLeaves)))
	{
		OutError = TEXT("Voxel recipe contains invalid default tree ecology symbols");
		return false;
	}
	if (InSettings.Grass.bEnabled && !IsValid(GrassPlant))
	{
		OutError = TEXT("Voxel recipe contains invalid default grass ecology symbol");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FVoxelSurfaceRuntimeRule::Validate(int32 InBlockCount, FString& OutError) const
{
	if (!Height.Validate(OutError) || !Slope.Validate(OutError) || !Temperature.Validate(OutError) || !Moisture.Validate(OutError))
	{
		return false;
	}
	if (MinDepth < 0 || MaxDepth < MinDepth)
	{
		OutError = TEXT("Voxel surface rule depth is invalid");
		return false;
	}
	if (BlockSymbol == MAX_uint16 || static_cast<int32>(BlockSymbol) >= InBlockCount)
	{
		OutError = TEXT("Voxel surface rule references an invalid block symbol");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelSurfaceRuntimeRuleSet::Validate(int32 InBlockCount, FString& OutError) const
{
	if (StableId.IsNone())
	{
		OutError = TEXT("Voxel surface rule set StableId is empty");
		return false;
	}
	for (const FVoxelSurfaceRuntimeRule& Rule : Rules)
	{
		if (!Rule.Validate(InBlockCount, OutError))
		{
			return false;
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelBiomeRuntimeDefinition::Validate(int32 InSurfaceRuleCount, int32 InFeatureCount, int32 InStructureCount, FString& OutError) const
{
	if (StableId.IsNone())
	{
		OutError = TEXT("Voxel biome StableId is empty");
		return false;
	}
	if (!Temperature.Validate(OutError) || !Moisture.Validate(OutError) || !Continentalness.Validate(OutError) ||
		!Erosion.Validate(OutError) || !Height.Validate(OutError) || !Slope.Validate(OutError))
	{
		return false;
	}
	if (SurfaceRuleIndex != INDEX_NONE && (SurfaceRuleIndex < 0 || SurfaceRuleIndex >= InSurfaceRuleCount))
	{
		OutError = TEXT("Voxel biome references an invalid surface rule set");
		return false;
	}
	if (!VoxelGenerationValidateIndices(FeatureIndices, InFeatureCount, TEXT("Biome FeatureIndices"), OutError) ||
		!VoxelGenerationValidateIndices(StructureIndices, InStructureCount, TEXT("Biome StructureIndices"), OutError))
	{
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelFeatureRuntimeDefinition::Validate(FString& OutError) const
{
	if (StableId.IsNone())
	{
		OutError = TEXT("Voxel feature StableId is empty");
		return false;
	}
	if (AlgorithmId.IsNone())
	{
		OutError = TEXT("Voxel feature AlgorithmId is empty");
		return false;
	}
	if (AlgorithmVersion == 0)
	{
		OutError = TEXT("Voxel feature algorithm version must be non-zero");
		return false;
	}
	if (Stage == EVoxelGenerationStage::None)
	{
		OutError = TEXT("Voxel feature generation stage cannot be None");
		return false;
	}

	return Placement.Validate(OutError);
}

bool FVoxelStructureRun::Validate(int32 InBlockCount, FString& OutError) const
{
	if (Length <= 0)
	{
		OutError = TEXT("Voxel structure run length must be positive");
		return false;
	}
	const uint16 Symbol = static_cast<uint16>(Value & 0xffffu);
	if (static_cast<int32>(Symbol) >= InBlockCount)
	{
		OutError = TEXT("Voxel structure run references an invalid block symbol");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelStructurePieceTemplate::Validate(int32 InBlockCount, FString& OutError) const
{
	if (StableId.IsNone())
	{
		OutError = TEXT("Voxel structure piece StableId is empty");
		return false;
	}
	if (SourceCellCentimeters <= 0)
	{
		OutError = TEXT("Voxel structure piece source cell size must be positive");
		return false;
	}
	if (!Bounds.IsValid())
	{
		OutError = TEXT("Voxel structure piece bounds are invalid");
		return false;
	}
	for (const FVoxelStructureRun& Run : Writes)
	{
		if (!Run.Validate(InBlockCount, OutError)) return false;
	}
	for (const FVoxelGenerationBounds& ClearVolume : ClearVolumes)
	{
		if (!ClearVolume.IsValid())
		{
			OutError = TEXT("Voxel structure clear volume is invalid");
			return false;
		}
	}
	for (const FVoxelStructureConnectorData& Connector : Connectors)
	{
		if (Connector.Type.IsNone())
		{
			OutError = TEXT("Voxel structure connector type is empty");
			return false;
		}
		if (Connector.Yaw > 3)
		{
			OutError = TEXT("Voxel structure connector yaw exceeds [0,3]");
			return false;
		}
	}
	for (const FVoxelStructureDetailRuntimeSocket& Detail : Details)
	{
		if (Detail.DetailId.IsNone() || Detail.Yaw > 3)
		{
			OutError = TEXT("Voxel structure detail socket is invalid");
			return false;
		}
	}

	OutError.Reset();
	return true;
}

bool FVoxelStructureRuntimeDefinition::Validate(int32 InBlockCount, FString& OutError) const
{
	if (StableId.IsNone())
	{
		OutError = TEXT("Voxel structure StableId is empty");
		return false;
	}
	if (Stage != EVoxelGenerationStage::UndergroundStructures && Stage != EVoxelGenerationStage::SurfaceStructures)
	{
		OutError = TEXT("Voxel structure must use an underground or surface structure stage");
		return false;
	}
	if (!Placement.Validate(OutError)) return false;
	if (Pieces.IsEmpty() && LayoutAlgorithmId.IsNone())
	{
		OutError = TEXT("Voxel structure has neither baked pieces nor a layout algorithm");
		return false;
	}
	for (const FVoxelStructurePieceTemplate& Piece : Pieces)
	{
		if (!Piece.Validate(InBlockCount, OutError)) return false;
	}
	if (!LayoutAlgorithmId.IsNone() && LayoutAlgorithmVersion == 0)
	{
		OutError = TEXT("Voxel structure layout algorithm version must be non-zero");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationRecipe::Validate(FString& OutError) const
{
	if (AlgorithmVersion == 0)
	{
		OutError = TEXT("Voxel recipe algorithm version must be non-zero");
		return false;
	}
	if (CellCentimeters <= 0 || CellCentimeters > 1000)
	{
		OutError = TEXT("Voxel recipe cell size is outside the supported range");
		return false;
	}
	if (!Settings.Validate(OutError)) return false;
	if (BlockNames.IsEmpty() || !BlockNames[0].IsNone())
	{
		OutError = TEXT("Voxel recipe BlockNames[0] must be reserved for Air");
		return false;
	}

	TSet<FName> UniqueBlocks;
	UniqueBlocks.Reserve(BlockNames.Num());
	for (int32 Index = 1; Index < BlockNames.Num(); ++Index)
	{
		const FName Name = BlockNames[Index];
		if (Name.IsNone())
		{
			OutError = TEXT("Voxel recipe contains an empty non-air block name");
			return false;
		}
		if (UniqueBlocks.Contains(Name))
		{
			OutError = FString::Printf(TEXT("Voxel recipe contains duplicate block name: %s"), *Name.ToString());
			return false;
		}
		UniqueBlocks.Add(Name);
	}
	if (!Palette.Validate(BlockNames.Num(), OutError)) return false;
	if (!Ecology.Validate(BlockNames.Num(), Settings.Ecology, OutError)) return false;
	if (!VoxelGenerationValidateUniqueNames(Biomes, TEXT("Voxel biomes"), OutError) ||
		!VoxelGenerationValidateUniqueNames(Features, TEXT("Voxel features"), OutError) ||
		!VoxelGenerationValidateUniqueNames(Structures, TEXT("Voxel structures"), OutError) ||
		!VoxelGenerationValidateUniqueNames(SurfaceRules, TEXT("Voxel surface rules"), OutError))
	{
		return false;
	}
	for (const FVoxelSurfaceRuntimeRuleSet& RuleSet : SurfaceRules)
	{
		if (!RuleSet.Validate(BlockNames.Num(), OutError)) return false;
	}
	for (const FVoxelFeatureRuntimeDefinition& Feature : Features)
	{
		if (!Feature.Validate(OutError)) return false;
	}
	for (const FVoxelStructureRuntimeDefinition& Structure : Structures)
	{
		if (!Structure.Validate(BlockNames.Num(), OutError)) return false;
	}
	for (const FVoxelBiomeRuntimeDefinition& Biome : Biomes)
	{
		if (!Biome.Validate(SurfaceRules.Num(), Features.Num(), Structures.Num(), OutError)) return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationRecipe::BuildLookups(FString& OutError)
{
	if (!Validate(OutError)) return false;
	BlockLookup.Reset();
	BiomeLookup.Reset();
	FeatureLookup.Reset();
	StructureLookup.Reset();
	SurfaceRuleLookup.Reset();
	for (int32 Index = 1; Index < BlockNames.Num(); ++Index) BlockLookup.Add(BlockNames[Index], Index);
	for (int32 Index = 0; Index < Biomes.Num(); ++Index) BiomeLookup.Add(Biomes[Index].StableId, Index);
	for (int32 Index = 0; Index < Features.Num(); ++Index) FeatureLookup.Add(Features[Index].StableId, Index);
	for (int32 Index = 0; Index < Structures.Num(); ++Index) StructureLookup.Add(Structures[Index].StableId, Index);
	for (int32 Index = 0; Index < SurfaceRules.Num(); ++Index) SurfaceRuleLookup.Add(SurfaceRules[Index].StableId, Index);
	OutError.Reset();
	return true;
}

int32 FVoxelGenerationRecipe::FindBlockSymbol(FName InStableName) const
{
	if (InStableName.IsNone()) return 0;
	const int32* Found = BlockLookup.Find(InStableName);
	return Found ? *Found : INDEX_NONE;
}

int32 FVoxelGenerationRecipe::FindBiome(FName InStableId) const
{
	const int32* Found = BiomeLookup.Find(InStableId);
	return Found ? *Found : INDEX_NONE;
}

int32 FVoxelGenerationRecipe::FindFeature(FName InStableId) const
{
	const int32* Found = FeatureLookup.Find(InStableId);
	return Found ? *Found : INDEX_NONE;
}

int32 FVoxelGenerationRecipe::FindStructure(FName InStableId) const
{
	const int32* Found = StructureLookup.Find(InStableId);
	return Found ? *Found : INDEX_NONE;
}

int32 FVoxelGenerationRecipe::FindSurfaceRules(FName InStableId) const
{
	const int32* Found = SurfaceRuleLookup.Find(InStableId);
	return Found ? *Found : INDEX_NONE;
}
