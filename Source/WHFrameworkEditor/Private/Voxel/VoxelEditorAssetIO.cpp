#include "Voxel/VoxelEditorAssetIO.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
UObject* FVoxelEditorAssetIO::LoadOrCreate(UClass* Class, const FString& PackageName, FString& Error)
{
	if (!Class || !FPackageName::IsValidLongPackageName(PackageName))
	{
		Error = TEXT("Invalid asset package path");
		return nullptr;
	}
	const FString Name = FPackageName::GetLongPackageAssetName(PackageName);
	const FString ObjectPath = PackageName + TEXT(".") + Name;
	if (UObject* Existing = LoadObject<UObject>(nullptr, *ObjectPath))
	{
		if (!Existing->IsA(Class))
		{
			Error = TEXT("Asset class conflict: ") + ObjectPath;
			return nullptr;
		}
		Existing->Modify();
		return Existing;
	}
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		Error = TEXT("CreatePackage failed: ") + PackageName;
		return nullptr;
	}
	UObject* Asset = NewObject<UObject>(Package, Class, *Name, RF_Public | RF_Standalone | RF_Transactional);
	if (!Asset)
	{
		Error = TEXT("NewObject failed: ") + ObjectPath;
		return nullptr;
	}
	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();
	return Asset;
}
bool FVoxelEditorAssetIO::Save(UObject* Asset, FString& Error)
{
	if (!Asset)
	{
		Error = TEXT("Cannot save a null asset");
		return false;
	}
	UPackage* Package = Asset->GetOutermost();
	const FString File = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true))
	{
		Error = TEXT("Cannot create asset output directory: ") + File;
		return false;
	}
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	Asset->MarkPackageDirty();
	if (!UPackage::SavePackage(Package, Asset, *File, Args))
	{
		Error = TEXT("Package save failed: ") + File;
		return false;
	}
	return true;
}
