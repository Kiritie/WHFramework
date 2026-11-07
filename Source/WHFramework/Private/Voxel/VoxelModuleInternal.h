#pragma once

namespace VoxelModuleInternal
{
	inline bool NormalizeVoxelWorldSettings(FVoxelGenerationSettings& InOutSettings, const EVoxelTerrainType InTerrainType, FString& OutError)
	{
		if (InTerrainType != EVoxelTerrainType::Default && InTerrainType != EVoxelTerrainType::Superflat)
		{
			OutError = TEXT("Voxel terrain type is invalid");
			return false;
		}
		if (InTerrainType == EVoxelTerrainType::Superflat)
		{
			if (InOutSettings.MaxZ <= 0)
			{
				OutError = TEXT("Superflat world bounds must contain its ground and open space");
				return false;
			}
			InOutSettings.MinZ = VoxelGeneration::SuperflatMinZ;
			InOutSettings.SeaLevel = FMath::Clamp(InOutSettings.SeaLevel, InOutSettings.MinZ, InOutSettings.MaxZ - 1);
			InOutSettings.BaseHeight = FMath::Clamp(InOutSettings.BaseHeight, InOutSettings.MinZ, InOutSettings.MaxZ - 1);
			InOutSettings.LavaCeiling = FMath::Clamp(InOutSettings.LavaCeiling, InOutSettings.MinZ, InOutSettings.MaxZ - 1);
		}
		return InOutSettings.Validate(OutError);
	}

	inline bool ValidateSuperflatGround(const EVoxelTerrainType InTerrainType,
		const FVoxelRegistrySnapshot* InRegistry, const FVoxelShapeRegistry* InShapes,
		const FVoxelGenerationRuntimeConfig& InConfig, FString& OutError)
	{
		if (InTerrainType != EVoxelTerrainType::Superflat) return true;
		if (!InRegistry || !InShapes)
		{
			OutError = TEXT("Voxel.SuperflatInvalidGrass: registry or collision shapes are unavailable");
			return false;
		}
		FString GroundError;
		if (!FVoxelWorldRuntime::ValidateSuperflatFoundation(*InRegistry, *InShapes, InConfig.Grass, GroundError))
		{
			OutError = FString::Printf(TEXT("Voxel.SuperflatInvalidGrass: %s"), *GroundError);
			return false;
		}
		return true;
	}

	inline FIntVector VoxelModuleToSection(const FIntVector& InPosition)
	{
		return FIntVector(
			VoxelGeneration::FloorDivide(InPosition.X, 16),
			VoxelGeneration::FloorDivide(InPosition.Y, 16),
			VoxelGeneration::FloorDivide(InPosition.Z, 16));
	}

	inline int32 VoxelModuleToCellIndex(const FIntVector& InPosition)
	{
		auto PositiveMod = [](const int32 InValue)
		{
			const int32 Result = InValue % 16;
			return Result < 0 ? Result + 16 : Result;
		};
		return PositiveMod(InPosition.X) +
			PositiveMod(InPosition.Y) * 16 +
			PositiveMod(InPosition.Z) * 256;
	}

	inline uint64 BuildGenerationSignature(
		const FVoxelWorldManifest& InManifest,
		const uint64 InContentSalt)
	{
		const uint64 Fingerprint =
			FVoxelManifestCodec::RecipeFingerprint(
				InManifest);

		if (Fingerprint == 0 ||
			InManifest.RegistryHash == 0 ||
			InManifest.RecipeHash == 0)
		{
			return 0;
		}

		const uint64 ContentIdentity = InContentSalt == 0
			? 0
			: VoxelBinary::Mix64(InContentSalt);
		const uint64 Signature =
			VoxelBinary::Mix64(
				Fingerprint ^
				VoxelBinary::Mix64(
					InManifest.RegistryHash) ^
				VoxelBinary::Mix64(
					InManifest.RecipeHash) ^
				ContentIdentity);

		return Signature == 0
			? 1
			: Signature;
	}

}
