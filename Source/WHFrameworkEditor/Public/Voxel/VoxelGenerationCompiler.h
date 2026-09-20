#pragma once

#include "CoreMinimal.h"

class UVoxelData;
class UVoxelStructureData;
class UVoxelWorldGenerationProfile;
struct FVoxelGenerationRecipe;
struct FVoxelRegistrySnapshot;
struct FVoxelStructurePieceTemplate;

class WHFRAMEWORKEDITOR_API FVoxelGenerationCompiler
{
public:
	static constexpr int32 CurrentBakeVersion = 2;

public:
	static bool Compile(UVoxelWorldGenerationProfile& InOutProfile, const FVoxelRegistrySnapshot& InRegistry, FString& OutError);
	static bool BuildRecipe(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		FVoxelGenerationRecipe& OutRecipe, FString& OutError);

private:
	static bool GatherBlockNames(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		TArray<FName>& OutBlockNames, FString& OutError);
	static bool CompilePalette(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		const TMap<FName, uint16>& InBlockSymbols, FVoxelGenerationRecipe& InOutRecipe, FString& OutError);
	static bool CompileSurfaceRules(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		const TMap<FName, uint16>& InBlockSymbols, FVoxelGenerationRecipe& InOutRecipe, FString& OutError);
	static bool CompileFeatures(const UVoxelWorldGenerationProfile& InProfile, FVoxelGenerationRecipe& InOutRecipe, FString& OutError);
	static bool CompileStructures(const UVoxelWorldGenerationProfile& InProfile, const FVoxelRegistrySnapshot& InRegistry,
		const TMap<FName, uint16>& InBlockSymbols, FVoxelGenerationRecipe& InOutRecipe, FString& OutError);
	static bool CompileBiomes(const UVoxelWorldGenerationProfile& InProfile, FVoxelGenerationRecipe& InOutRecipe, FString& OutError);
	static bool CompileStructurePiece(const UVoxelStructureData& InStructure, const FVoxelRegistrySnapshot& InRegistry,
		const TMap<FName, uint16>& InBlockSymbols, int32 InTargetCellCentimeters,
		FVoxelStructurePieceTemplate& OutPiece, FString& OutError);
	static bool ResolveBlockName(const UVoxelData* InVoxelData, const FVoxelRegistrySnapshot& InRegistry,
		FName& OutBlockName, FString& OutError);
	static bool ResolveBlockName(const FPrimaryAssetId& InAssetId, const FVoxelRegistrySnapshot& InRegistry,
		FName& OutBlockName, FString& OutError);
	static bool ResolveBlockSymbol(const UVoxelData* InVoxelData, const FVoxelRegistrySnapshot& InRegistry,
		const TMap<FName, uint16>& InBlockSymbols, uint16& OutSymbol, FString& OutError);
	static bool AddReferencedVoxel(const UVoxelData* InVoxelData, const FVoxelRegistrySnapshot& InRegistry,
		TSet<FName>& InOutNames, FString& OutError);
};
