#include "Voxel/Generation/VoxelBuiltinFeatures.h"

#include "Voxel/Generation/VoxelGenerationMath.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Voxel/Voxels/Data/VoxelData.h"

const FName FVoxelOreVeinFeatureAlgorithm::AlgorithmId(TEXT("Voxel.OreVein"));

namespace
{
	constexpr int32 MaxConfigBytes = 4096;

	bool ValidateOre(const FVoxelOreVeinRuntimeConfig& InConfig, FString& OutError)
	{
		if (InConfig.OreSymbol == 0 || InConfig.OreSymbol == MAX_uint16 ||
			InConfig.HostSymbols.IsEmpty() || InConfig.HostSymbols.Contains(0) ||
			InConfig.HostSymbols.Contains(MAX_uint16) || InConfig.MinLength < 1 ||
			InConfig.MaxLength < InConfig.MinLength || InConfig.MaxLength > 256 ||
			InConfig.Radius < 0 || InConfig.Radius > 4)
		{
			OutError = TEXT("Voxel ore vein configuration is invalid");
			return false;
		}
		OutError.Reset();
		return true;
	}

	bool EncodeOre(const FVoxelOreVeinRuntimeConfig& InConfig, TArray<uint8>& OutBytes, FString& OutError)
	{
		if (!ValidateOre(InConfig, OutError) || InConfig.HostSymbols.Num() > 64)
		{
			return false;
		}
		FVoxelByteWriter Writer(MaxConfigBytes);
		Writer.U16(InConfig.OreSymbol);
		Writer.U8(static_cast<uint8>(InConfig.HostSymbols.Num()));
		for (const uint16 Symbol : InConfig.HostSymbols)
		{
			Writer.U16(Symbol);
		}
		Writer.I32(InConfig.MinLength);
		Writer.I32(InConfig.MaxLength);
		Writer.I32(InConfig.Radius);
		return Writer.Finish(OutBytes);
	}

	bool DecodeOre(TConstArrayView<uint8> InBytes, FVoxelOreVeinRuntimeConfig& OutConfig, FString& OutError)
	{
		FVoxelByteReader Reader(InBytes);
		FVoxelOreVeinRuntimeConfig Config;
		Config.OreSymbol = Reader.U16();
		const uint8 HostCount = Reader.U8();
		Config.HostSymbols.SetNum(HostCount);
		for (uint16& Symbol : Config.HostSymbols)
		{
			Symbol = Reader.U16();
		}
		Config.MinLength = Reader.I32();
		Config.MaxLength = Reader.I32();
		Config.Radius = Reader.I32();
		if (!Reader.End() || !ValidateOre(Config, OutError))
		{
			return false;
		}
		OutConfig = MoveTemp(Config);
		return true;
	}

	void InitializeInstance(const FVoxelFeatureQueryContext& InContext, FVoxelFeatureInstance& OutInstance)
	{
		OutInstance = FVoxelFeatureInstance();
		OutInstance.DefinitionId = InContext.Definition->StableId;
		OutInstance.Anchor = InContext.CandidateAnchor;
		OutInstance.Id = VoxelGeneration::MakeStableId(
			InContext.WorldSeed, InContext.CandidateAnchor, InContext.Definition->StableHash);
	}
}

FName FVoxelOreVeinFeatureAlgorithm::GetId() const
{
	return AlgorithmId;
}

uint32 FVoxelOreVeinFeatureAlgorithm::GetVersion() const
{
	return 1;
}

bool FVoxelOreVeinFeatureAlgorithm::GatherReferencedBlocks(
	const FParameter& InConfiguration,
	TArray<FPrimaryAssetId>& OutBlockAssets,
	FString& OutError) const
{
	const FVoxelOreVeinFeatureConfig* Config = InConfiguration.GetPtr<FVoxelOreVeinFeatureConfig>();
	if (!Config)
	{
		OutError = TEXT("Voxel ore vein authoring configuration type is invalid");
		return false;
	}
	UVoxelData* Ore = Config->Ore.LoadSynchronous();
	if (!Ore)
	{
		OutError = TEXT("Voxel ore vein has no valid ore block");
		return false;
	}
	OutBlockAssets.Reset();
	OutBlockAssets.Add(Ore->GetPrimaryAssetId());
	for (const TSoftObjectPtr<UVoxelData>& HostSoft : Config->Hosts)
	{
		UVoxelData* Host = HostSoft.LoadSynchronous();
		if (!Host)
		{
			OutError = TEXT("Voxel ore vein contains an invalid host block");
			return false;
		}
		OutBlockAssets.AddUnique(Host->GetPrimaryAssetId());
	}
	OutError.Reset();
	return true;
}

bool FVoxelOreVeinFeatureAlgorithm::BakeConfiguration(
	const FVoxelFeatureBakeContext& InContext,
	const FParameter& InConfiguration,
	TArray<uint8>& OutBytes,
	FString& OutError) const
{
	const FVoxelOreVeinFeatureConfig* Authoring = InConfiguration.GetPtr<FVoxelOreVeinFeatureConfig>();
	if (!Authoring)
	{
		OutError = TEXT("Voxel ore vein authoring configuration type is invalid");
		return false;
	}
	UVoxelData* Ore = Authoring->Ore.LoadSynchronous();
	if (!Ore)
	{
		OutError = TEXT("Voxel ore vein has no valid ore asset");
		return false;
	}
	FVoxelOreVeinRuntimeConfig Runtime;
	if (!InContext.ResolveBlockSymbol(Ore->GetPrimaryAssetId(), Runtime.OreSymbol, OutError))
	{
		return false;
	}
	for (const TSoftObjectPtr<UVoxelData>& HostSoft : Authoring->Hosts)
	{
		UVoxelData* Host = HostSoft.LoadSynchronous();
		if (!Host)
		{
			OutError = TEXT("Voxel ore vein contains an invalid host asset");
			return false;
		}
		uint16 HostSymbol = MAX_uint16;
		if (!InContext.ResolveBlockSymbol(Host->GetPrimaryAssetId(), HostSymbol, OutError))
		{
			return false;
		}
		Runtime.HostSymbols.AddUnique(HostSymbol);
	}
	Runtime.MinLength = Authoring->MinLength;
	Runtime.MaxLength = Authoring->MaxLength;
	Runtime.Radius = Authoring->Radius;
	return EncodeOre(Runtime, OutBytes, OutError);
}

bool FVoxelOreVeinFeatureAlgorithm::Generate(
	const FVoxelFeatureQueryContext& InContext,
	const TConstArrayView<uint8> InConfigBytes,
	FVoxelFeatureInstance& OutInstance,
	FString& OutError) const
{
	FVoxelOreVeinRuntimeConfig Config;
	if (!InContext.Definition || !DecodeOre(InConfigBytes, Config, OutError))
	{
		if (!InContext.Definition)
		{
			OutError = TEXT("Voxel ore vein has no runtime definition");
		}
		return false;
	}
	const uint64 Seed = VoxelGeneration::MakeSeed(
		InContext.WorldSeed, InContext.CandidateAnchor, InContext.Definition->StableHash);
	const int32 Length = VoxelGeneration::RandomRange(Seed, Config.MinLength, Config.MaxLength);
	static const FIntVector Directions[] =
	{
		FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
		FIntVector(0, 1, 0), FIntVector(0, -1, 0),
		FIntVector(0, 0, 1), FIntVector(0, 0, -1)
	};
	FVoxelFeatureInstance Instance;
	InitializeInstance(InContext, Instance);
	FIntVector Center = InContext.CandidateAnchor;
	TSet<FIntVector> Written;
	for (int32 Step = 0; Step < Length; ++Step)
	{
		for (int32 Z = -Config.Radius; Z <= Config.Radius; ++Z)
		{
			for (int32 Y = -Config.Radius; Y <= Config.Radius; ++Y)
			{
				for (int32 X = -Config.Radius; X <= Config.Radius; ++X)
				{
					if (X * X + Y * Y + Z * Z > Config.Radius * Config.Radius)
					{
						continue;
					}
					const FIntVector Position = Center + FIntVector(X, Y, Z);
					uint32 BaseSymbol = 0;
					if (!Written.Contains(Position) && InContext.SampleBaseSymbol(Position, BaseSymbol) &&
						Config.HostSymbols.Contains(static_cast<uint16>(BaseSymbol & 0xffffu)))
					{
						Written.Add(Position);
						Instance.Writes.Add({Position, Config.OreSymbol});
					}
				}
			}
		}
		const int32 DirectionIndex = VoxelGeneration::RandomRange(
			VoxelGeneration::Mix(Seed + static_cast<uint64>(Step)), 0, UE_ARRAY_COUNT(Directions) - 1);
		Center += Directions[DirectionIndex];
	}
	OutInstance = MoveTemp(Instance);
	OutError.Reset();
	return true;
}

bool VoxelBuiltinFeatures::Register(FString& OutError)
{
	FVoxelFeatureAlgorithmRegistry& Registry = FVoxelFeatureAlgorithmRegistry::Get();
	if (const TSharedPtr<const IVoxelFeatureAlgorithm, ESPMode::ThreadSafe> Existing =
		Registry.Find(FVoxelOreVeinFeatureAlgorithm::AlgorithmId))
	{
		if (Existing->GetVersion() != 1)
		{
			OutError = TEXT("Voxel builtin OreVein algorithm version conflict");
			return false;
		}
		OutError.Reset();
		return true;
	}
	return Registry.Register(
		MakeShared<FVoxelOreVeinFeatureAlgorithm, ESPMode::ThreadSafe>(), OutError);
}
