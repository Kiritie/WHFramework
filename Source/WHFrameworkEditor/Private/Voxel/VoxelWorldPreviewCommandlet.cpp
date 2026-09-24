#include "Voxel/VoxelWorldPreviewCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Voxel/Authoring/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelBuiltinFeatures.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "Voxel/Generation/VoxelGenerationPipeline.h"
#include "Voxel/Generation/Climate/VoxelClimateGenerator.h"
#include "Voxel/Generation/Terrain/VoxelTerrainGenerator.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/Data/VoxelData.h"

UVoxelWorldPreviewCommandlet::UVoxelWorldPreviewCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UVoxelWorldPreviewCommandlet::Main(const FString& Params)
{
	FString ProfilePath;
	if (!FParse::Value(*Params, TEXT("Profile="), ProfilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Required: -Profile=/Game/...Asset.Asset"));
		return 1;
	}

	int32 OriginX = -4096;
	int32 OriginY = -4096;
	int32 Width = 256;
	int32 Height = 256;
	int32 Step = 32;
	FParse::Value(*Params, TEXT("OriginX="), OriginX);
	FParse::Value(*Params, TEXT("OriginY="), OriginY);
	FParse::Value(*Params, TEXT("Width="), Width);
	FParse::Value(*Params, TEXT("Height="), Height);
	FParse::Value(*Params, TEXT("Step="), Step);
	if (Width <= 0 || Height <= 0 || Width > 512 || Height > 512 || Step <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Preview grid must be 1..512 by 1..512 with positive step"));
		return 1;
	}
	const int64 LastX = static_cast<int64>(OriginX) +
		static_cast<int64>(Width - 1) * Step;
	const int64 LastY = static_cast<int64>(OriginY) +
		static_cast<int64>(Height - 1) * Step;
	if (LastX >= MAX_int32 || LastY >= MAX_int32)
	{
		UE_LOG(LogTemp, Error, TEXT("Preview grid exceeds valid world coordinates"));
		return 1;
	}
	const bool bMacroOnly = FParse::Param(*Params, TEXT("MacroOnly"));

	FString OutputPath = FPaths::ProjectSavedDir() /
		TEXT("WorldArchitecture/TerrainPreview.csv");
	FParse::Value(*Params, TEXT("Output="), OutputPath);
	UVoxelWorldGenerationProfile* Profile =
		LoadObject<UVoxelWorldGenerationProfile>(nullptr, *ProfilePath);
	if (!Profile)
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot load generation profile: %s"), *ProfilePath);
		return 2;
	}

	FString Error;
	if (!VoxelBuiltinFeatures::Register(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Builtin feature registration failed: %s"), *Error);
		return 3;
	}

	IAssetRegistry& AssetRegistry =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.SearchAllAssets(true);
	FARFilter Filter;
	Filter.ClassPaths.Add(UVoxelData::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(TEXT("/WHFramework/Voxel/DataAssets"));
	Filter.PackagePaths.Add(TEXT("/Game/DataAssets/Voxel"));
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);
	TArray<UVoxelData*> Blocks;
	Blocks.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		if (UVoxelData* Data = Cast<UVoxelData>(Asset.GetAsset()))
		{
			Blocks.Add(Data);
		}
	}

	FVoxelRegistry Registry;
	if (!Registry.Build(Blocks, false, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Voxel registry failed: %s"), *Error);
		return 4;
	}
	FVoxelGenerationRuntimeConfig MutableConfig;
	if (!FVoxelGenerationBinding::Build(*Profile, *Registry.GetSnapshot(),
		Profile->Defaults, Profile->TargetCellCentimeters, MutableConfig, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Generation binding failed: %s"), *Error);
		return 5;
	}

	const TSharedRef<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe> Config =
		MakeShared<const FVoxelGenerationRuntimeConfig, ESPMode::ThreadSafe>(MoveTemp(MutableConfig));
	TArray<FVoxelEnvironmentSample> Samples;
	if (!bMacroOnly)
	{
		const TSharedRef<FVoxelGenerationPlanCache, ESPMode::ThreadSafe> Cache =
			MakeShared<FVoxelGenerationPlanCache, ESPMode::ThreadSafe>();
		const FVoxelGenerationPipeline Pipeline(Config, Cache);
		if (!Pipeline.SampleEnvironments(FIntPoint(OriginX, OriginY), Width,
			Height, Step, Samples, Error, nullptr, false))
		{
			UE_LOG(LogTemp, Error, TEXT("Environment sampling failed: %s"), *Error);
			return 6;
		}
	}

	TArray<FString> Rows;
	Rows.Reserve(Width * Height + 1);
	Rows.Add(TEXT("x,y,surface_z,water_z,slope_permille,landform,plain_q15,hills_q15,highland_q15,mountain_q15,plateau_q15,basin_q15,river,lake,ocean,coast,river_distance,bank_distance,floodplain_q15,river_id_high,river_id_low"));
	const TSharedRef<const FVoxelClimateGenerator, ESPMode::ThreadSafe> Climate =
		MakeShared<const FVoxelClimateGenerator, ESPMode::ThreadSafe>(Config->Recipe.ToSharedRef());
	const FVoxelTerrainGenerator Terrain(Config->Recipe.ToSharedRef(), Climate);
	for (int32 Index = 0; Index < Width * Height; ++Index)
	{
		const int64 X = static_cast<int64>(OriginX) +
			static_cast<int64>(Index % Width) * Step;
		const int64 Y = static_cast<int64>(OriginY) +
			static_cast<int64>(Index / Width) * Step;
		FVoxelColumnSample MacroColumn;
		if (bMacroOnly)
		{
			const FVoxelMacroTerrainSample Macro = Terrain.SampleMacro(
				static_cast<int32>(X), static_cast<int32>(Y));
			MacroColumn.SurfaceZ = Macro.SurfaceZ;
			MacroColumn.SlopePermille = Macro.SlopePermille;
			MacroColumn.Landform = Macro.Landform;
			MacroColumn.SurfaceWaterZ = Config->Recipe->Settings.SeaLevel;
			MacroColumn.bOcean = Macro.SurfaceZ < Config->Recipe->Settings.SeaLevel;
		}
		const FVoxelColumnSample& Column = bMacroOnly ?
			MacroColumn : Samples[Index].Column;
		const FVoxelLandformSample& Landform = Column.Landform;
		Rows.Add(FString::Printf(
			TEXT("%lld,%lld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%llu,%llu"),
			X, Y, Column.SurfaceZ, Column.SurfaceWaterZ, Column.SlopePermille,
			static_cast<int32>(Landform.Dominant), Landform.PlainQ15,
			Landform.HillsQ15, Landform.HighlandQ15, Landform.MountainQ15,
			Landform.PlateauQ15, Landform.BasinQ15,
			Column.bRiver ? 1 : 0, Column.bLake ? 1 : 0,
			Column.bOcean ? 1 : 0, Column.bCoast ? 1 : 0,
			Column.RiverDistanceCells, Column.BankDistanceCells,
			Column.FloodplainStrengthQ15,
			Column.RiverId.High, Column.RiverId.Low));
	}
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPath), true) ||
		!FFileHelper::SaveStringArrayToFile(Rows, *OutputPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTemp, Error, TEXT("Cannot write preview: %s"), *OutputPath);
		return 7;
	}
	UE_LOG(LogTemp, Display, TEXT("Exported %d %s samples to %s"),
		Width * Height, bMacroOnly ? TEXT("macro terrain") :
		TEXT("natural environment"), *OutputPath);
	return 0;
}
