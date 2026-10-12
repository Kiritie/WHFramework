#include "Task/TaskGraphMigrationCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Task/Base/TaskAsset.h"
#include "Task/Graph/TaskGraphOperations.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"

UTaskGraphMigrationCommandlet::UTaskGraphMigrationCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UTaskGraphMigrationCommandlet::Main(const FString& Parameters)
{
	FString RootPath = TEXT("/Game");
	FParse::Value(*Parameters, TEXT("Root="), RootPath);
	const bool bWrite = FParse::Param(*Parameters, TEXT("Save"));
	FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	Registry.Get().SearchAllAssets(true);
	FARFilter Filter;
	Filter.ClassPaths.Add(UTaskAsset::StaticClass()->GetClassPathName());
	Filter.PackagePaths.Add(FName(*RootPath));
	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = true;
	TArray<FAssetData> Data;
	Registry.Get().GetAssets(Filter, Data);
	Data.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.GetSoftObjectPath().ToString() < Right.GetSoftObjectPath().ToString();
	});
	TArray<TStrongObjectPtr<UTaskAsset>> Assets;
	int32 Converted = 0;
	for(const FAssetData& Item : Data)
	{
		UTaskAsset* Asset = Cast<UTaskAsset>(Item.GetAsset());
		if(!Asset)
		{
			UE_LOG(LogTemp, Error, TEXT("无法加载任务资产：%s"), *Item.GetSoftObjectPath().ToString());
			return 1;
		}
		Assets.Emplace(Asset);
		if(!Asset->StartGUID.IsValid())
		{
			TArray<FText> Errors;
			if(!FTaskGraphOperations::MigrateAsset(Asset, Errors, false))
			{
				for(const FText& Error : Errors)
				{
					UE_LOG(LogTemp, Error, TEXT("%s: %s"), *Asset->GetPathName(), *Error.ToString());
				}
				return 1;
			}
			++Converted;
		}
	}
	for(const TStrongObjectPtr<UTaskAsset>& Asset : Assets)
	{
		TArray<FText> Errors;
		if(!Asset->CompileTasks(Errors))
		{
			for(const FText& Error : Errors)
			{
				UE_LOG(LogTemp, Error, TEXT("%s"), *Error.ToString());
			}
			return 1;
		}
	}
	if(!bWrite)
	{
		UE_LOG(LogTemp, Display, TEXT("检查通过：资产 %d，需迁移 %d。未写入磁盘；加 -Save 执行备份并保存。"), Assets.Num(), Converted);
		return 0;
	}
	const FString BackupRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/TaskGraphMigration"), FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")));
	for(const TStrongObjectPtr<UTaskAsset>& Asset : Assets)
	{
		const FString PackageName = Asset->GetOutermost()->GetName();
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if(IFileManager::Get().FileExists(*Filename))
		{
			const FString BackupFilename = FPaths::Combine(BackupRoot, PackageName.RightChop(1) + FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(BackupFilename), true);
			if(IFileManager::Get().Copy(*BackupFilename, *Filename, true, true) != COPY_OK)
			{
				UE_LOG(LogTemp, Error, TEXT("备份失败，未开始保存：%s"), *Filename);
				return 1;
			}
		}
	}
	for(const TStrongObjectPtr<UTaskAsset>& Asset : Assets)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if(!UPackage::SavePackage(Asset->GetOutermost(), Asset.Get(), *Filename, Args))
		{
			UE_LOG(LogTemp, Error, TEXT("保存失败：%s；原文件备份：%s"), *Filename, *BackupRoot);
			return 1;
		}
	}
	UE_LOG(LogTemp, Display, TEXT("任务图迁移完成：资产 %d，新增 Start %d，备份 %s"), Assets.Num(), Converted, *BackupRoot);
	return 0;
}
