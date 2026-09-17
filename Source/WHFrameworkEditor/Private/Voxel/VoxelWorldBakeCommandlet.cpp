#include "Voxel/VoxelWorldBakeCommandlet.h"
#include "Voxel/VoxelGenerationCompiler.h"
#include "Voxel/VoxelDetailBaker.h"
#include "Voxel/Generation/Assets/VoxelWorldGenerationProfile.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"
UVoxelWorldBakeCommandlet::UVoxelWorldBakeCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}
int32 UVoxelWorldBakeCommandlet::Main(const FString& Params)
{
    FString Path,Root=TEXT("/Game/VoxelGenerated/Phase2/Details"),ReportPath=FPaths::ProjectSavedDir()/TEXT("VoxelPhase2AssetReport.txt");
    if(!FParse::Value(*Params,TEXT("Profile="),Path)){UE_LOG(LogTemp,Error,TEXT("Required: -Profile=/Game/...Asset.Asset"));return 1;}
    FParse::Value(*Params,TEXT("GeneratedRoot="),Root);FParse::Value(*Params,TEXT("Report="),ReportPath);
    const bool ValidateOnly=FParse::Param(*Params,TEXT("ValidateOnly"));
    auto* P=LoadObject<UVoxelWorldGenerationProfile>(nullptr,*Path);
    if(!P){UE_LOG(LogTemp,Error,TEXT("Cannot load generation profile: %s"),*Path);return 2;}
    FString E;TArray<FString> Report;
    if(!ValidateOnly)for(const auto& D:P->Details)
    {auto* A=D.LoadSynchronous();if(!A||!FVoxelDetailBaker::Bake(*A,Root,E)){UE_LOG(LogTemp,Error,TEXT("Detail bake: %s"),*E);return 3;}}
    auto& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();AR.SearchAllAssets(true);
    FARFilter Filter;Filter.ClassPaths.Add(UVoxelData::StaticClass()->GetClassPathName());Filter.bRecursiveClasses=true;
    Filter.PackagePaths.Add(TEXT("/WHFramework/Voxel/DataAssets"));Filter.PackagePaths.Add(TEXT("/Game/DataAssets/Voxel"));Filter.bRecursivePaths=true;
    TArray<FAssetData> Found;AR.GetAssets(Filter,Found);TArray<UVoxelData*> Blocks;
    for(const FAssetData& A:Found)if(auto* D=Cast<UVoxelData>(A.GetAsset()))Blocks.Add(D);
    FVoxelRegistry Registry;
    if(!Registry.Build(Blocks,false,E)){UE_LOG(LogTemp,Error,TEXT("Registry: %s"),*E);return 4;}
    const bool OK=FVoxelGenerationCompiler::Compile(*P,*Registry.GetSnapshot(),Report,E,!ValidateOnly);
    Report.Add(OK?TEXT("PASS asset validation/compilation"):TEXT("FAIL ")+E);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true);
    if(!FFileHelper::SaveStringArrayToFile(Report,*ReportPath)){UE_LOG(LogTemp,Error,TEXT("Cannot write asset report"));return 5;}
    UE_LOG(LogTemp,Display,TEXT("Voxel phase2 asset result=%s report=%s"),OK?TEXT("PASS"):TEXT("FAIL"),*ReportPath);
    return OK?0:6;
}
