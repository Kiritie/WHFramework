#include "Voxel/VoxelBakeCommandlet.h"
#include "Voxel/VoxelAssetMigrationTool.h"
#include "Voxel/VoxelTextureArrayBuilder.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Misc/Parse.h"
#include "AssetCompilingManager.h"
UVoxelBakeCommandlet::UVoxelBakeCommandlet(){IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;ShowErrorCount=true;}
int32 UVoxelBakeCommandlet::Main(const FString& Params)
{
    FString Plan,SetPath=TEXT("/Game/VoxelGenerated/DA_VoxelMaterials"),Error;FVoxelArrayBuildOptions O;
    FParse::Value(*Params,TEXT("Plan="),Plan);FParse::Value(*Params,TEXT("Set="),SetPath);
    FParse::Value(*Params,TEXT("Out="),O.OutputRoot);FParse::Value(*Params,TEXT("Tile="),O.TileSize);
    FParse::Value(*Params,TEXT("MaxSlices="),O.MaxSlicesPerBank);
    const bool Check=FParse::Param(*Params,TEXT("CheckOnly"));TArray<UVoxelData*> Assets;
    bool OK=Plan.IsEmpty()?FVoxelAssetMigrationTool::Scan(Assets,Error):FVoxelAssetMigrationTool::ApplyPlan(Plan,!Check,Assets,Error);
    if(!OK){UE_LOG(LogTemp,Error,TEXT("Voxel migration failed: %s"),*Error);return 1;}
    if(Check){UE_LOG(LogTemp,Display,TEXT("Voxel plan/definition preflight passed: %d assets"),Assets.Num());return 0;}
    // Scan again so a partial plan cannot accidentally publish a MaterialSet that omits other assets.
    if(!FVoxelAssetMigrationTool::Scan(Assets,Error)){UE_LOG(LogTemp,Error,TEXT("Voxel scan failed: %s"),*Error);return 2;}
    auto* Set=Cast<UVoxelMaterialSet>(FVoxelEditorAssetIO::LoadOrCreate(UVoxelMaterialSet::StaticClass(),SetPath,Error));
    if(!Set||!FVoxelTextureArrayBuilder::Build(Assets,*Set,O,Error)){UE_LOG(LogTemp,Error,TEXT("Voxel bake failed: %s"),*Error);return 3;}
    FAssetCompilingManager::Get().FinishAllCompilation();
    for(UVoxelData* A:Assets)if(!A->ValidateDefinition(true,Error)){UE_LOG(LogTemp,Error,TEXT("Voxel bake validation failed: %s"),*Error);return 4;}
    if(!Set->Validate(Error)){UE_LOG(LogTemp,Error,TEXT("MaterialSet validation failed: %s"),*Error);return 5;}
    UE_LOG(LogTemp,Display,TEXT("Voxel bake saved: %d assets, %d banks; target Cook still required"),Assets.Num(),Set->Banks.Num());return 0;
}
