#include "Widget/WidgetEditorToolset.h"

#include "GameplayTagsManager.h"
#include "EditorReimportHandler.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "UObject/ObjectRedirector.h"
#include "Widget/Common/WidgetBlueprintEditorLibrary.h"
#include "Widget/Theme/WidgetThemeEditorLibrary.h"

bool UWidgetEditorToolset::FixupWidgetRedirectors(const TArray<FString>& PackageNames)
{
	if (PackageNames.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("An explicit widget package list is required."));
		return false;
	}
	TArray<UPackage*> Packages;
	TArray<UObjectRedirector*> Redirectors;
	for (const FString& PackageName : PackageNames)
	{
		if (!FPackageName::IsValidLongPackageName(PackageName) ||
			(!PackageName.StartsWith(TEXT("/Game/Blueprints/Widget/")) &&
			 !PackageName.StartsWith(TEXT("/WHFramework/Blueprints/Widget/")) &&
			 !PackageName.StartsWith(TEXT("/WHFrameworkUtilities/Blueprints/Widget/"))))
		{
			UKismetSystemLibrary::RaiseScriptError(TEXT("Only explicit widget asset packages can be cleaned."));
			return false;
		}
		UPackage* Package = FindPackage(nullptr, *PackageName);
		if (!Package && FPackageName::DoesPackageExist(PackageName))
		{
			Package = LoadPackage(nullptr, *PackageName, LOAD_None);
		}
		if (Package)
		{
			Package->FullyLoad();
			FString UnexpectedAsset;
			ForEachObjectWithPackage(Package, [&Redirectors, &UnexpectedAsset](UObject* Object)
			{
				if (Object->IsAsset())
				{
					if (UObjectRedirector* Redirector = Cast<UObjectRedirector>(Object))
					{
						Redirectors.AddUnique(Redirector);
					}
					else
					{
						UnexpectedAsset = Object->GetPathName();
						return false;
					}
				}
				return true;
			}, EGetObjectsFlags::None, RF_Transient | RF_ClassDefaultObject, EInternalObjectFlags::Garbage);
			if (!UnexpectedAsset.IsEmpty())
			{
				UKismetSystemLibrary::RaiseScriptError(FString::Printf(TEXT("Package contains a non-redirector asset: %s"), *UnexpectedAsset));
				return false;
			}
			Packages.AddUnique(Package);
		}
	}
	if (!Redirectors.IsEmpty())
	{
		FAssetToolsModule::GetModule().Get().FixupReferencers(Redirectors, false, ERedirectFixupMode::DeleteFixedUpRedirectors);
		return !FAssetToolsModule::GetModule().Get().IsFixupReferencersInProgress();
	}
	ObjectTools::CleanupAfterSuccessfulDelete(Packages, false);
	for (const FString& PackageName : PackageNames)
	{
		if (FPackageName::DoesPackageExist(PackageName))
		{
			return false;
		}
	}
	return true;
}

FString UWidgetEditorToolset::GetBlueprintGraphData(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("A source Blueprint is required."));
		return FString();
	}
	return UWidgetBlueprintEditorLibrary::ExportGraphData(Blueprint);
}

FString UWidgetEditorToolset::GetWidgetPreviewData(UBlueprint* Blueprint)
{
	FString Error;
	FString Result = UWidgetBlueprintEditorLibrary::GetPreviewData(Blueprint, Error);
	if (!Error.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return Result;
}

UWidget* UWidgetEditorToolset::CreateWidgetTemplate(UBlueprint* Blueprint, TSubclassOf<UWidget> WidgetClass, FName WidgetName)
{
	FString Error;
	UWidget* Widget = UWidgetBlueprintEditorLibrary::CreateWidgetTemplate(Blueprint, WidgetClass, WidgetName, Error);
	if (!Widget)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return Widget;
}

bool UWidgetEditorToolset::SetRootWidget(UBlueprint* Blueprint, UWidget* Root)
{
	FString Error;
	const bool bSuccess = UWidgetBlueprintEditorLibrary::SetRootWidget(Blueprint, Root, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::MoveWidget(UBlueprint* Blueprint, UWidget* Widget, UPanelWidget* Parent, int32 ChildIndex)
{
	FString Error;
	const bool bSuccess = UWidgetBlueprintEditorLibrary::MoveWidget(Blueprint, Widget, Parent, ChildIndex, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::ReplaceWidget(UBlueprint* Blueprint, UWidget* Widget, TSubclassOf<UWidget> WidgetClass)
{
	FString Error;
	const bool bSuccess = UWidgetBlueprintEditorLibrary::ReplaceWidget(Blueprint, Widget, WidgetClass, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::RemoveWidget(UBlueprint* Blueprint, UWidget* Widget)
{
	FString Error;
	const bool bSuccess = UWidgetBlueprintEditorLibrary::RemoveWidget(Blueprint, Widget, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::RetargetWidgetEvent(UBlueprint* Blueprint, FName ComponentName, UClass* DelegateOwnerClass, FName DelegateName)
{
	FString Error;
	const bool bSuccess = UWidgetBlueprintEditorLibrary::RetargetWidgetEvent(Blueprint, ComponentName, DelegateOwnerClass, DelegateName, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::CompileWidgetBlueprint(UBlueprint* Blueprint)
{
	FString Error;
	const bool bSuccess = UWidgetBlueprintEditorLibrary::CompileWidgetBlueprint(Blueprint, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

UBlueprint* UWidgetEditorToolset::CreateStyle(UWidgetTheme* Theme, const FString& Tag, UClass* ParentClass, const FString& DestinationRoot)
{
	FString Error;
	UBlueprint* Style = UWidgetThemeEditorLibrary::CreateStyle(Theme, Tag, ParentClass, DestinationRoot, Error);
	if (!Style)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return Style;
}

bool UWidgetEditorToolset::SetStyleProperty(UWidgetTheme* Theme, const FString& Tag, FName Property, const FString& Value)
{
	const FGameplayTag StyleTag = UGameplayTagsManager::Get().RequestGameplayTag(FName(*Tag), false);
	if (!StyleTag.IsValid())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("The style Tag must be registered in its owning module."));
		return false;
	}
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::SetStyleProperty(Theme, StyleTag, Property, Value, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::RemapThemeTextStyleTag(UWidgetTheme* Theme, const FString& OldTag, const FString& NewTag)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::RemapThemeTextStyleTag(Theme, OldTag, NewTag, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::ReimportTextureSource(UTexture2D* Texture, const FString& SourceFile)
{
	if (!Texture)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("An existing Texture2D asset is required."));
		return false;
	}

	const FString ReimportFile = FPaths::ConvertRelativePathToFull(SourceFile);
	if (!FPaths::GetExtension(ReimportFile).Equals(TEXT("png"), ESearchCase::IgnoreCase) || IFileManager::Get().FileSize(*ReimportFile) == INDEX_NONE)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("The reimport source must be an existing PNG file."));
		return false;
	}

	UPackage* Package = Texture->GetOutermost();
	if (!Package || Package->IsDirty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Save or discard pending changes to the texture before reimporting it."));
		return false;
	}

	FReimportManager* ReimportManager = FReimportManager::Instance();
	TArray<FString> ExistingSources;
	if (!ReimportManager || !ReimportManager->CanReimport(Texture, &ExistingSources) || ExistingSources.Num() != 1)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("The Texture2D asset does not have one supported reimport source."));
		return false;
	}

	const bool bSucceeded = ReimportManager->Reimport(Texture, false, false, ReimportFile, nullptr, 0, false, true, false);
	if (!bSucceeded)
	{
		ReimportManager->UpdateReimportPaths(Texture, ExistingSources);
		UKismetSystemLibrary::RaiseScriptError(TEXT("Texture reimport failed; the original reimport source path was restored."));
		return false;
	}

	return true;
}

bool UWidgetEditorToolset::MigrateStyleTag(const FString& OldTag, const FString& NewTag)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::MigrateTag(OldTag, NewTag, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::MigrateStyleTagReferences(const FString& OldTagName, const FString& NewTag)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::MigrateTagReferences(OldTagName, NewTag, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::MigrateStyleTagAlias(const FString& OldAlias, const FString& NewTag)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::MigrateTagAlias(OldAlias, NewTag, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::RemoveStyleBinding(UWidgetTheme* Theme, const FString& Tag)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::RemoveStyleBinding(Theme, Tag, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::RemoveDuplicateGlobalStyleTag(const FString& Tag, const FString& TargetOwnerPath)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::RemoveDuplicateGlobalTagDefinition(Tag, TargetOwnerPath, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::SaveTheme(UWidgetTheme* Theme)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::SaveTheme(Theme, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}

bool UWidgetEditorToolset::SetProjectDefault(UWidgetTheme* Theme)
{
	FString Error;
	const bool bSuccess = UWidgetThemeEditorLibrary::SetProjectDefault(Theme, Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}
