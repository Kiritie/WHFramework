#include "Voxel/Generation/VoxelGenerationRecipeCodec.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace
{
	constexpr uint32 VoxelRecipeMagic = 0x31524356;
	constexpr uint32 VoxelRecipeSchemaVersion = 4;
	constexpr int32 MaxRecipeBytes = 32 * 1024 * 1024;
	constexpr int32 MaxRecipeArrayCount = 1 << 20;

	void WriteName(FVoxelByteWriter& Writer, FName Name)
	{
		Writer.String(Name.IsNone() ? FString() : Name.ToString(), 1024);
	}

	FName ReadName(FVoxelByteReader& Reader)
	{
		const FString Value = Reader.String(1024);
		return Value.IsEmpty() ? NAME_None : FName(*Value);
	}

	void WriteVector(FVoxelByteWriter& Writer, const FIntVector& Value)
	{
		Writer.I32(Value.X);
		Writer.I32(Value.Y);
		Writer.I32(Value.Z);
	}

	FIntVector ReadVector(FVoxelByteReader& Reader)
	{
		const int32 X = Reader.I32();
		const int32 Y = Reader.I32();
		const int32 Z = Reader.I32();
		return FIntVector(X, Y, Z);
	}

	void WriteRange(FVoxelByteWriter& Writer, const FVoxelGenerationRange& Value)
	{
		Writer.I32(Value.Min);
		Writer.I32(Value.Max);
	}

	FVoxelGenerationRange ReadRange(FVoxelByteReader& Reader)
	{
		FVoxelGenerationRange Result;
		Result.Min = Reader.I32();
		Result.Max = Reader.I32();
		return Result;
	}

	void WriteTreeSettings(FVoxelByteWriter& Writer, const FVoxelTreeGenerationSettings& Value)
	{
		Writer.U8(Value.bEnabled ? 1 : 0);
		Writer.I32(Value.MinHeight);
		Writer.I32(Value.MaxHeight);
		Writer.I32(Value.Spacing);
		Writer.I32(Value.DensityPermille);
		Writer.I32(Value.ChancePermille);
		Writer.I32(Value.CrownRadius);
		Writer.I32(Value.MaxSlopePermille);
		WriteRange(Writer, Value.Temperature);
		WriteRange(Writer, Value.Moisture);
		Writer.U8(Value.bAllowNearWater ? 1 : 0);
	}

	FVoxelTreeGenerationSettings ReadTreeSettings(FVoxelByteReader& Reader)
	{
		FVoxelTreeGenerationSettings Result;
		Result.bEnabled = Reader.U8() != 0;
		Result.MinHeight = Reader.I32();
		Result.MaxHeight = Reader.I32();
		Result.Spacing = Reader.I32();
		Result.DensityPermille = Reader.I32();
		Result.ChancePermille = Reader.I32();
		Result.CrownRadius = Reader.I32();
		Result.MaxSlopePermille = Reader.I32();
		Result.Temperature = ReadRange(Reader);
		Result.Moisture = ReadRange(Reader);
		Result.bAllowNearWater = Reader.U8() != 0;
		return Result;
	}

	void WriteGrassSettings(FVoxelByteWriter& Writer, const FVoxelGrassGenerationSettings& Value)
	{
		Writer.U8(Value.bEnabled ? 1 : 0);
		Writer.I32(Value.Spacing);
		Writer.I32(Value.DensityPermille);
		Writer.I32(Value.ChancePermille);
		Writer.I32(Value.PatchRadius);
		Writer.I32(Value.PatchFillPermille);
		Writer.I32(Value.MaxSlopePermille);
		WriteRange(Writer, Value.Temperature);
		WriteRange(Writer, Value.Moisture);
		Writer.U8(Value.bAllowNearWater ? 1 : 0);
	}

	FVoxelGrassGenerationSettings ReadGrassSettings(FVoxelByteReader& Reader)
	{
		FVoxelGrassGenerationSettings Result;
		Result.bEnabled = Reader.U8() != 0;
		Result.Spacing = Reader.I32();
		Result.DensityPermille = Reader.I32();
		Result.ChancePermille = Reader.I32();
		Result.PatchRadius = Reader.I32();
		Result.PatchFillPermille = Reader.I32();
		Result.MaxSlopePermille = Reader.I32();
		Result.Temperature = ReadRange(Reader);
		Result.Moisture = ReadRange(Reader);
		Result.bAllowNearWater = Reader.U8() != 0;
		return Result;
	}

	void WriteBounds(FVoxelByteWriter& Writer, const FVoxelGenerationBounds& Value)
	{
		WriteVector(Writer, Value.Min);
		WriteVector(Writer, Value.Max);
	}

	FVoxelGenerationBounds ReadBounds(FVoxelByteReader& Reader)
	{
		FVoxelGenerationBounds Result;
		Result.Min = ReadVector(Reader);
		Result.Max = ReadVector(Reader);
		return Result;
	}

	void WriteSettings(FVoxelByteWriter& Writer, const FVoxelGenerationSettings& Value)
	{
		Writer.I32(Value.Seed);
		Writer.I32(Value.MinZ);
		Writer.I32(Value.MaxZ);
		Writer.I32(Value.SeaLevel);
		Writer.I32(Value.BaseHeight);
		Writer.I32(Value.ContinentalPeriod);
		Writer.I32(Value.ErosionPeriod);
		Writer.I32(Value.MountainPeriod);
		Writer.I32(Value.ClimatePeriod);
		Writer.I32(Value.DetailPeriod);
		Writer.I32(Value.ContinentalAmplitude);
		Writer.I32(Value.MountainAmplitude);
		Writer.I32(Value.DetailAmplitude);
		Writer.I32(Value.Landform.DomainPeriod);
		Writer.I32(Value.Landform.ReliefPeriod);
		Writer.I32(Value.Landform.HillsPeriod);
		Writer.I32(Value.Landform.PlateauPeriod);
		Writer.I32(Value.Landform.DomainWarpCells);
		Writer.I32(Value.Landform.PlainRelief);
		Writer.I32(Value.Landform.HillRelief);
		Writer.I32(Value.Landform.HighlandUplift);
		Writer.I32(Value.Landform.PlateauUplift);
		Writer.I32(Value.Landform.BasinDepth);
		Writer.I32(Value.HydrologyCellSize);
		Writer.I32(Value.HydrologyRegionSide);
		Writer.I32(Value.RiverSourceAccumulation);
		Writer.I32(Value.RiverBaseHalfWidth);
		Writer.I32(Value.RiverBaseDepth);
		Writer.I32(Value.HydrologyHaloCells);
		Writer.I32(Value.HydrologySinkSpacing);
		Writer.I32(Value.RiverSourceSpacing);
		Writer.I32(Value.RiverTraceBudget);
		Writer.I32(Value.LakeMaxCells);
		Writer.I32(Value.CaveSpacing);
		Writer.I32(Value.CaveMinDepth);
		Writer.I32(Value.CaveMaxDepth);
		Writer.I32(Value.CaveMainRadius);
		Writer.I32(Value.CaveBranchRadius);
		Writer.I32(Value.CaveSystemChancePermille);
		Writer.I32(Value.CaveEntranceChancePermille);
		Writer.I32(Value.CaveEntranceLength);
		Writer.I32(Value.CaveEntranceDropPerStep);
		Writer.I32(Value.CaveEntranceTransitionDepth);
		Writer.I32(Value.CaveRoomChancePermille);
		Writer.I32(Value.CaveBranchChancePermille);
		Writer.I32(Value.AquiferSpacing);
		Writer.I32(Value.AquiferRadius);
		Writer.I32(Value.LavaCeiling);
		WriteTreeSettings(Writer, Value.Ecology.Tree);
		WriteGrassSettings(Writer, Value.Ecology.Grass);
	}

	FVoxelGenerationSettings ReadSettings(FVoxelByteReader& Reader)
	{
		FVoxelGenerationSettings Result;
		Result.Seed = Reader.I32();
		Result.MinZ = Reader.I32();
		Result.MaxZ = Reader.I32();
		Result.SeaLevel = Reader.I32();
		Result.BaseHeight = Reader.I32();
		Result.ContinentalPeriod = Reader.I32();
		Result.ErosionPeriod = Reader.I32();
		Result.MountainPeriod = Reader.I32();
		Result.ClimatePeriod = Reader.I32();
		Result.DetailPeriod = Reader.I32();
		Result.ContinentalAmplitude = Reader.I32();
		Result.MountainAmplitude = Reader.I32();
		Result.DetailAmplitude = Reader.I32();
		Result.Landform.DomainPeriod = Reader.I32();
		Result.Landform.ReliefPeriod = Reader.I32();
		Result.Landform.HillsPeriod = Reader.I32();
		Result.Landform.PlateauPeriod = Reader.I32();
		Result.Landform.DomainWarpCells = Reader.I32();
		Result.Landform.PlainRelief = Reader.I32();
		Result.Landform.HillRelief = Reader.I32();
		Result.Landform.HighlandUplift = Reader.I32();
		Result.Landform.PlateauUplift = Reader.I32();
		Result.Landform.BasinDepth = Reader.I32();
		Result.HydrologyCellSize = Reader.I32();
		Result.HydrologyRegionSide = Reader.I32();
		Result.RiverSourceAccumulation = Reader.I32();
		Result.RiverBaseHalfWidth = Reader.I32();
		Result.RiverBaseDepth = Reader.I32();
		Result.HydrologyHaloCells = Reader.I32();
		Result.HydrologySinkSpacing = Reader.I32();
		Result.RiverSourceSpacing = Reader.I32();
		Result.RiverTraceBudget = Reader.I32();
		Result.LakeMaxCells = Reader.I32();
		Result.CaveSpacing = Reader.I32();
		Result.CaveMinDepth = Reader.I32();
		Result.CaveMaxDepth = Reader.I32();
		Result.CaveMainRadius = Reader.I32();
		Result.CaveBranchRadius = Reader.I32();
		Result.CaveSystemChancePermille = Reader.I32();
		Result.CaveEntranceChancePermille = Reader.I32();
		Result.CaveEntranceLength = Reader.I32();
		Result.CaveEntranceDropPerStep = Reader.I32();
		Result.CaveEntranceTransitionDepth = Reader.I32();
		Result.CaveRoomChancePermille = Reader.I32();
		Result.CaveBranchChancePermille = Reader.I32();
		Result.AquiferSpacing = Reader.I32();
		Result.AquiferRadius = Reader.I32();
		Result.LavaCeiling = Reader.I32();
		Result.Ecology.Tree = ReadTreeSettings(Reader);
		Result.Ecology.Grass = ReadGrassSettings(Reader);
		return Result;
	}

	bool ReadCount(FVoxelByteReader& Reader, int32& OutCount)
	{
		const uint32 Count = Reader.U32();
		if (!Reader.IsValid() || Count > static_cast<uint32>(MaxRecipeArrayCount))
		{
			Reader.Reject();
			return false;
		}
		OutCount = static_cast<int32>(Count);
		return true;
	}

	void WriteFeaturePlacement(FVoxelByteWriter& Writer, const FVoxelFeaturePlacement& Value)
	{
		Writer.I32(Value.Spacing);
		Writer.I32(Value.ChancePermille);
		Writer.I32(Value.MinZ);
		Writer.I32(Value.MaxZ);
		Writer.I32(Value.MaxSlopePermille);
		Writer.U8(Value.bRequireSurface);
		Writer.U8(Value.bRequireSolidFloor);
		Writer.U8(Value.bAllowNearWater);
	}

	FVoxelFeaturePlacement ReadFeaturePlacement(FVoxelByteReader& Reader)
	{
		FVoxelFeaturePlacement Result;
		Result.Spacing = Reader.I32();
		Result.ChancePermille = Reader.I32();
		Result.MinZ = Reader.I32();
		Result.MaxZ = Reader.I32();
		Result.MaxSlopePermille = Reader.I32();
		Result.bRequireSurface = Reader.U8() != 0;
		Result.bRequireSolidFloor = Reader.U8() != 0;
		Result.bAllowNearWater = Reader.U8() != 0;
		return Result;
	}

	void WriteStructurePlacement(FVoxelByteWriter& Writer, const FVoxelStructurePlacement& Value)
	{
		Writer.I32(Value.Spacing);
		Writer.I32(Value.Separation);
		Writer.I32(Value.ChancePermille);
		Writer.I32(Value.MinZ);
		Writer.I32(Value.MaxZ);
		Writer.I32(Value.MaxSlopePermille);
		Writer.U8(Value.bRequireSurface);
		Writer.I32(Value.MaxTerrainAdjustment);
		Writer.I32(Value.BlendRadius);
	}

	FVoxelStructurePlacement ReadStructurePlacement(FVoxelByteReader& Reader)
	{
		FVoxelStructurePlacement Result;
		Result.Spacing = Reader.I32();
		Result.Separation = Reader.I32();
		Result.ChancePermille = Reader.I32();
		Result.MinZ = Reader.I32();
		Result.MaxZ = Reader.I32();
		Result.MaxSlopePermille = Reader.I32();
		Result.bRequireSurface = Reader.U8() != 0;
		Result.MaxTerrainAdjustment = Reader.I32();
		Result.BlendRadius = Reader.I32();
		return Result;
	}
}

bool FVoxelGenerationRecipeCodec::Encode(const FVoxelGenerationRecipe& Recipe, TArray<uint8>& OutBytes, FString& OutError)
{
	if (!Recipe.Validate(OutError))
	{
		return false;
	}

	FVoxelByteWriter Writer(MaxRecipeBytes);
	Writer.U32(VoxelRecipeMagic);
	Writer.U32(VoxelRecipeSchemaVersion);
	Writer.U32(Recipe.AlgorithmVersion);
	Writer.I32(Recipe.CellCentimeters);
	WriteSettings(Writer, Recipe.Settings);
	Writer.U32(Recipe.BlockNames.Num());
	for (const FName Name : Recipe.BlockNames) WriteName(Writer, Name);
	Writer.U16(Recipe.Palette.Air);
	Writer.U16(Recipe.Palette.Stone);
	Writer.U16(Recipe.Palette.Dirt);
	Writer.U16(Recipe.Palette.Grass);
	Writer.U16(Recipe.Palette.Sand);
	Writer.U16(Recipe.Palette.Snow);
	Writer.U16(Recipe.Palette.Water);
	Writer.U16(Recipe.Palette.Lava);
	Writer.U16(Recipe.Palette.Bedrock);
	Writer.U16(Recipe.Palette.Road);
	Writer.U16(Recipe.Ecology.TreeTrunk);
	Writer.U16(Recipe.Ecology.TreeLeaves);
	Writer.U16(Recipe.Ecology.GrassPlant);

	Writer.U32(Recipe.SurfaceRules.Num());
	for (const FVoxelSurfaceRuntimeRuleSet& RuleSet : Recipe.SurfaceRules)
	{
		WriteName(Writer, RuleSet.StableId);
		Writer.U32(RuleSet.Rules.Num());
		for (const FVoxelSurfaceRuntimeRule& Rule : RuleSet.Rules)
		{
			WriteRange(Writer, Rule.Height);
			WriteRange(Writer, Rule.Slope);
			WriteRange(Writer, Rule.Temperature);
			WriteRange(Writer, Rule.Moisture);
			Writer.I32(Rule.MinDepth);
			Writer.I32(Rule.MaxDepth);
			Writer.U16(Rule.BlockSymbol);
			const uint8 Flags =
				uint8(Rule.bRiverOnly ? 1 : 0) |
				uint8(Rule.bLakeOnly ? 2 : 0) |
				uint8(Rule.bOceanOnly ? 4 : 0) |
				uint8(Rule.bCoastOnly ? 8 : 0);
			Writer.U8(Flags);
		}
	}

	Writer.U32(Recipe.Features.Num());
	for (const FVoxelFeatureRuntimeDefinition& Feature : Recipe.Features)
	{
		WriteName(Writer, Feature.StableId);
		Writer.U64(Feature.StableHash);
		WriteName(Writer, Feature.AlgorithmId);
		Writer.U32(Feature.AlgorithmVersion);
		Writer.U8(static_cast<uint8>(Feature.Stage));
		WriteFeaturePlacement(Writer, Feature.Placement);
		Writer.Blob(Feature.ConfigBytes, 256 * 1024);
	}

	Writer.U32(Recipe.Structures.Num());
	for (const FVoxelStructureRuntimeDefinition& Structure : Recipe.Structures)
	{
		WriteName(Writer, Structure.StableId);
		Writer.U64(Structure.StableHash);
		Writer.U8(static_cast<uint8>(Structure.Stage));
		WriteStructurePlacement(Writer, Structure.Placement);
		Writer.U8(static_cast<uint8>(Structure.Adaptation));
		WriteName(Writer, Structure.LayoutAlgorithmId);
		Writer.U32(Structure.LayoutAlgorithmVersion);
		Writer.U32(Structure.Pieces.Num());
		for (const FVoxelStructurePieceTemplate& Piece : Structure.Pieces)
		{
			WriteName(Writer, Piece.StableId);
			Writer.I32(Piece.SourceCellCentimeters);
			Writer.I32(Piece.GroundZ);
			WriteBounds(Writer, Piece.Bounds);
			WriteVector(Writer, Piece.Entrance);
			Writer.U32(Piece.Writes.Num());
			for (const FVoxelStructureRun& Run : Piece.Writes)
			{
				WriteVector(Writer, Run.Start);
				Writer.I32(Run.Length);
				Writer.U32(Run.Value);
			}
			Writer.U32(Piece.ClearVolumes.Num());
			for (const FVoxelGenerationBounds& Bounds : Piece.ClearVolumes) WriteBounds(Writer, Bounds);
			Writer.U32(Piece.Connectors.Num());
			for (const FVoxelStructureConnectorData& Connector : Piece.Connectors)
			{
				WriteName(Writer, Connector.Type);
				WriteVector(Writer, Connector.Position);
				Writer.U8(Connector.Yaw);
			}
			Writer.U32(Piece.Details.Num());
			for (const FVoxelStructureDetailRuntimeSocket& Detail : Piece.Details)
			{
				WriteName(Writer, Detail.DetailId);
				WriteVector(Writer, Detail.SourceCorner);
				Writer.U8(Detail.Yaw);
			}
		}
	}

	Writer.U32(Recipe.Biomes.Num());
	for (const FVoxelBiomeRuntimeDefinition& Biome : Recipe.Biomes)
	{
		WriteName(Writer, Biome.StableId);
		Writer.U64(Biome.StableHash);
		Writer.I32(Biome.Priority);
		WriteRange(Writer, Biome.Temperature);
		WriteRange(Writer, Biome.Moisture);
		WriteRange(Writer, Biome.Continentalness);
		WriteRange(Writer, Biome.Erosion);
		WriteRange(Writer, Biome.Height);
		WriteRange(Writer, Biome.Slope);
		Writer.I32(Biome.SurfaceRuleIndex);
		Writer.U32(Biome.FeatureIndices.Num());
		for (const int32 Index : Biome.FeatureIndices) Writer.I32(Index);
		Writer.U32(Biome.StructureIndices.Num());
		for (const int32 Index : Biome.StructureIndices) Writer.I32(Index);
	}

	if (!Writer.Finish(OutBytes))
	{
		OutError = TEXT("Voxel recipe exceeds the canonical byte budget");
		return false;
	}

	OutError.Reset();
	return true;
}

bool FVoxelGenerationRecipeCodec::Decode(TConstArrayView<uint8> Bytes, FVoxelGenerationRecipe& OutRecipe, FString& OutError)
{
	if (Bytes.IsEmpty() || Bytes.Num() > MaxRecipeBytes)
	{
		OutError = TEXT("Voxel recipe byte buffer is invalid");
		return false;
	}

	FVoxelByteReader Reader(Bytes);
	if (Reader.U32() != VoxelRecipeMagic || Reader.U32() != VoxelRecipeSchemaVersion)
	{
		OutError = TEXT("Voxel recipe header is invalid or unsupported");
		return false;
	}

	FVoxelGenerationRecipe Result;
	Result.AlgorithmVersion = Reader.U32();
	Result.CellCentimeters = Reader.I32();
	Result.Settings = ReadSettings(Reader);
	int32 Count = 0;
	if (!ReadCount(Reader, Count)) return false;
	for (int32 Index = 0; Index < Count; ++Index) Result.BlockNames.Add(ReadName(Reader));
	Result.Palette.Air = Reader.U16();
	Result.Palette.Stone = Reader.U16();
	Result.Palette.Dirt = Reader.U16();
	Result.Palette.Grass = Reader.U16();
	Result.Palette.Sand = Reader.U16();
	Result.Palette.Snow = Reader.U16();
	Result.Palette.Water = Reader.U16();
	Result.Palette.Lava = Reader.U16();
	Result.Palette.Bedrock = Reader.U16();
	Result.Palette.Road = Reader.U16();
	Result.Ecology.TreeTrunk = Reader.U16();
	Result.Ecology.TreeLeaves = Reader.U16();
	Result.Ecology.GrassPlant = Reader.U16();

	if (!ReadCount(Reader, Count)) return false;
	for (int32 SetIndex = 0; SetIndex < Count; ++SetIndex)
	{
		FVoxelSurfaceRuntimeRuleSet& RuleSet = Result.SurfaceRules.AddDefaulted_GetRef();
		RuleSet.StableId = ReadName(Reader);
		int32 RuleCount = 0;
		if (!ReadCount(Reader, RuleCount)) return false;
		for (int32 RuleIndex = 0; RuleIndex < RuleCount; ++RuleIndex)
		{
			FVoxelSurfaceRuntimeRule& Rule = RuleSet.Rules.AddDefaulted_GetRef();
			Rule.Height = ReadRange(Reader);
			Rule.Slope = ReadRange(Reader);
			Rule.Temperature = ReadRange(Reader);
			Rule.Moisture = ReadRange(Reader);
			Rule.MinDepth = Reader.I32();
			Rule.MaxDepth = Reader.I32();
			Rule.BlockSymbol = Reader.U16();
			const uint8 Flags = Reader.U8();
			Rule.bRiverOnly = Flags & 1;
			Rule.bLakeOnly = Flags & 2;
			Rule.bOceanOnly = Flags & 4;
			Rule.bCoastOnly = Flags & 8;
		}
	}

	if (!ReadCount(Reader, Count)) return false;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVoxelFeatureRuntimeDefinition& Feature = Result.Features.AddDefaulted_GetRef();
		Feature.StableId = ReadName(Reader);
		Feature.StableHash = Reader.U64();
		Feature.AlgorithmId = ReadName(Reader);
		Feature.AlgorithmVersion = Reader.U32();
		Feature.Stage = static_cast<EVoxelGenerationStage>(Reader.U8());
		Feature.Placement = ReadFeaturePlacement(Reader);
		Feature.ConfigBytes = Reader.Blob(256 * 1024);
	}

	if (!ReadCount(Reader, Count)) return false;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVoxelStructureRuntimeDefinition& Structure = Result.Structures.AddDefaulted_GetRef();
		Structure.StableId = ReadName(Reader);
		Structure.StableHash = Reader.U64();
		Structure.Stage = static_cast<EVoxelGenerationStage>(Reader.U8());
		Structure.Placement = ReadStructurePlacement(Reader);
		Structure.Adaptation = static_cast<EVoxelStructureAdaptation>(Reader.U8());
		Structure.LayoutAlgorithmId = ReadName(Reader);
		Structure.LayoutAlgorithmVersion = Reader.U32();
		int32 PieceCount = 0;
		if (!ReadCount(Reader, PieceCount)) return false;
		for (int32 PieceIndex = 0; PieceIndex < PieceCount; ++PieceIndex)
		{
			FVoxelStructurePieceTemplate& Piece = Structure.Pieces.AddDefaulted_GetRef();
			Piece.StableId = ReadName(Reader);
			Piece.SourceCellCentimeters = Reader.I32();
			Piece.GroundZ = Reader.I32();
			Piece.Bounds = ReadBounds(Reader);
			Piece.Entrance = ReadVector(Reader);
			int32 ItemCount = 0;
			if (!ReadCount(Reader, ItemCount)) return false;
			for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
			{
				FVoxelStructureRun& Run = Piece.Writes.AddDefaulted_GetRef();
				Run.Start = ReadVector(Reader);
				Run.Length = Reader.I32();
				Run.Value = Reader.U32();
			}
			if (!ReadCount(Reader, ItemCount)) return false;
			for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex) Piece.ClearVolumes.Add(ReadBounds(Reader));
			if (!ReadCount(Reader, ItemCount)) return false;
			for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
			{
				FVoxelStructureConnectorData& Connector = Piece.Connectors.AddDefaulted_GetRef();
				Connector.Type = ReadName(Reader);
				Connector.Position = ReadVector(Reader);
				Connector.Yaw = Reader.U8();
			}
			if (!ReadCount(Reader, ItemCount)) return false;
			for (int32 ItemIndex = 0; ItemIndex < ItemCount; ++ItemIndex)
			{
				FVoxelStructureDetailRuntimeSocket& Detail = Piece.Details.AddDefaulted_GetRef();
				Detail.DetailId = ReadName(Reader);
				Detail.SourceCorner = ReadVector(Reader);
				Detail.Yaw = Reader.U8();
			}
		}
	}

	if (!ReadCount(Reader, Count)) return false;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FVoxelBiomeRuntimeDefinition& Biome = Result.Biomes.AddDefaulted_GetRef();
		Biome.StableId = ReadName(Reader);
		Biome.StableHash = Reader.U64();
		Biome.Priority = Reader.I32();
		Biome.Temperature = ReadRange(Reader);
		Biome.Moisture = ReadRange(Reader);
		Biome.Continentalness = ReadRange(Reader);
		Biome.Erosion = ReadRange(Reader);
		Biome.Height = ReadRange(Reader);
		Biome.Slope = ReadRange(Reader);
		Biome.SurfaceRuleIndex = Reader.I32();
		int32 IndexCount = 0;
		if (!ReadCount(Reader, IndexCount)) return false;
		for (int32 Item = 0; Item < IndexCount; ++Item) Biome.FeatureIndices.Add(Reader.I32());
		if (!ReadCount(Reader, IndexCount)) return false;
		for (int32 Item = 0; Item < IndexCount; ++Item) Biome.StructureIndices.Add(Reader.I32());
	}

	if (!Reader.End())
	{
		OutError = TEXT("Voxel recipe contains invalid trailing or truncated data");
		return false;
	}
	Result.RecipeHash = Hash(Bytes);
	if (!Result.BuildLookups(OutError)) return false;
	OutRecipe = MoveTemp(Result);
	OutError.Reset();
	return true;
}

bool FVoxelGenerationRecipeCodec::RefreshHash(FVoxelGenerationRecipe& Recipe, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!Encode(Recipe, Bytes, OutError)) return false;
	Recipe.RecipeHash = Hash(Bytes);
	OutError.Reset();
	return true;
}

uint64 FVoxelGenerationRecipeCodec::Hash(TConstArrayView<uint8> Bytes)
{
	return VoxelBinary::Hash(Bytes);
}
