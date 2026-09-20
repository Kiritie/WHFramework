#include "Voxel/Generation/VoxelManifestCodec.h"

#include "Voxel/Serialization/VoxelBinaryCodec.h"

namespace
{
	constexpr uint32 ManifestMagic = 0x344D5856;

	void WriteManifestSettings(FVoxelByteWriter& InWriter, const FVoxelGenerationSettings& InSettings)
	{
		InWriter.I32(InSettings.Seed);
		InWriter.I32(InSettings.MinZ);
		InWriter.I32(InSettings.MaxZ);
		InWriter.I32(InSettings.SeaLevel);
		InWriter.I32(InSettings.BaseHeight);
		InWriter.I32(InSettings.ContinentalPeriod);
		InWriter.I32(InSettings.ErosionPeriod);
		InWriter.I32(InSettings.MountainPeriod);
		InWriter.I32(InSettings.ClimatePeriod);
		InWriter.I32(InSettings.DetailPeriod);
		InWriter.I32(InSettings.ContinentalAmplitude);
		InWriter.I32(InSettings.MountainAmplitude);
		InWriter.I32(InSettings.DetailAmplitude);
		InWriter.I32(InSettings.HydrologyCellSize);
		InWriter.I32(InSettings.HydrologyRegionSide);
		InWriter.I32(InSettings.RiverSourceAccumulation);
		InWriter.I32(InSettings.RiverBaseHalfWidth);
		InWriter.I32(InSettings.RiverBaseDepth);
		InWriter.I32(InSettings.HydrologyHaloCells);
		InWriter.I32(InSettings.HydrologySinkSpacing);
		InWriter.I32(InSettings.RiverSourceSpacing);
		InWriter.I32(InSettings.RiverTraceBudget);
		InWriter.I32(InSettings.LakeMaxCells);
		InWriter.I32(InSettings.CaveSpacing);
		InWriter.I32(InSettings.CaveMinDepth);
		InWriter.I32(InSettings.CaveMaxDepth);
		InWriter.I32(InSettings.CaveMainRadius);
		InWriter.I32(InSettings.CaveBranchRadius);
		InWriter.I32(InSettings.AquiferSpacing);
		InWriter.I32(InSettings.AquiferRadius);
		InWriter.I32(InSettings.LavaCeiling);
	}

	void ReadSettings(FVoxelByteReader& InReader, FVoxelGenerationSettings& OutSettings)
	{
		OutSettings.Seed = InReader.I32();
		OutSettings.MinZ = InReader.I32();
		OutSettings.MaxZ = InReader.I32();
		OutSettings.SeaLevel = InReader.I32();
		OutSettings.BaseHeight = InReader.I32();
		OutSettings.ContinentalPeriod = InReader.I32();
		OutSettings.ErosionPeriod = InReader.I32();
		OutSettings.MountainPeriod = InReader.I32();
		OutSettings.ClimatePeriod = InReader.I32();
		OutSettings.DetailPeriod = InReader.I32();
		OutSettings.ContinentalAmplitude = InReader.I32();
		OutSettings.MountainAmplitude = InReader.I32();
		OutSettings.DetailAmplitude = InReader.I32();
		OutSettings.HydrologyCellSize = InReader.I32();
		OutSettings.HydrologyRegionSide = InReader.I32();
		OutSettings.RiverSourceAccumulation = InReader.I32();
		OutSettings.RiverBaseHalfWidth = InReader.I32();
		OutSettings.RiverBaseDepth = InReader.I32();
		OutSettings.HydrologyHaloCells = InReader.I32();
		OutSettings.HydrologySinkSpacing = InReader.I32();
		OutSettings.RiverSourceSpacing = InReader.I32();
		OutSettings.RiverTraceBudget = InReader.I32();
		OutSettings.LakeMaxCells = InReader.I32();
		OutSettings.CaveSpacing = InReader.I32();
		OutSettings.CaveMinDepth = InReader.I32();
		OutSettings.CaveMaxDepth = InReader.I32();
		OutSettings.CaveMainRadius = InReader.I32();
		OutSettings.CaveBranchRadius = InReader.I32();
		OutSettings.AquiferSpacing = InReader.I32();
		OutSettings.AquiferRadius = InReader.I32();
		OutSettings.LavaCeiling = InReader.I32();
	}

	void WriteGenerationIdentity(FVoxelByteWriter& InWriter, const FVoxelWorldManifest& InManifest)
	{
		InWriter.U32(InManifest.GeneratorVersion);
		InWriter.I32(InManifest.BlockSizeCentimeters);
		WriteManifestSettings(InWriter, InManifest.Settings);
	}
}

uint64 FVoxelManifestCodec::RecipeFingerprint(const FVoxelWorldManifest& InManifest)
{
	FVoxelByteWriter Writer(MaxBytes);
	WriteGenerationIdentity(Writer, InManifest);
	TArray<uint8> Bytes;
	return Writer.Finish(Bytes) ? VoxelBinary::Hash(Bytes) : 0;
}

bool FVoxelManifestCodec::Encode(
	const FVoxelWorldManifest& InManifest,
	TArray<uint8>& OutBytes)
{
	FString Error;
	if (!InManifest.WorldId.IsValid() ||
		InManifest.GeneratorVersion == 0 ||
		InManifest.BlockSizeCentimeters <= 0 ||
		InManifest.RegistryHash == 0 ||
		InManifest.RecipeHash == 0 ||
		InManifest.BaseSampleHash == 0 ||
		!InManifest.Settings.Validate(Error))
	{
		return false;
	}

	FVoxelByteWriter Writer(MaxBytes);
	Writer.U32(ManifestMagic);
	Writer.U32(FVoxelWorldManifest::ProtocolVersion);
	Writer.Guid(InManifest.WorldId);
	WriteGenerationIdentity(Writer, InManifest);
	Writer.U64(InManifest.RegistryHash);
	Writer.U64(InManifest.RecipeHash);
	Writer.U64(InManifest.BaseSampleHash);
	return Writer.Finish(OutBytes);
}

bool FVoxelManifestCodec::Decode(
	TConstArrayView<uint8> InBytes,
	FVoxelWorldManifest& OutManifest)
{
	if (InBytes.Num() > MaxBytes)
	{
		return false;
	}
	FVoxelByteReader Reader(InBytes);
	if (Reader.U32() != ManifestMagic || Reader.U32() != FVoxelWorldManifest::ProtocolVersion)
	{
		return false;
	}

	FVoxelWorldManifest Manifest;
	Manifest.WorldId = Reader.Guid();
	Manifest.GeneratorVersion = Reader.U32();
	Manifest.BlockSizeCentimeters = Reader.I32();
	ReadSettings(Reader, Manifest.Settings);
	Manifest.RegistryHash = Reader.U64();
	Manifest.RecipeHash = Reader.U64();
	Manifest.BaseSampleHash = Reader.U64();
	FString Error;
	if (!Reader.End() ||
		!Manifest.WorldId.IsValid() ||
		Manifest.GeneratorVersion == 0 ||
		Manifest.BlockSizeCentimeters <= 0 ||
		Manifest.RegistryHash == 0 ||
		Manifest.RecipeHash == 0 ||
		Manifest.BaseSampleHash == 0 ||
		!Manifest.Settings.Validate(Error))
	{
		return false;
	}

	OutManifest = MoveTemp(Manifest);
	return true;
}
