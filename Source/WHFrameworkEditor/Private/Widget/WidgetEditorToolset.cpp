#include "Widget/WidgetEditorToolset.h"

#include "GameplayTagsManager.h"
#include "EditorReimportHandler.h"
#include "HAL/FileManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "UObject/Package.h"
#include "Widget/Common/WidgetBlueprintEditorLibrary.h"
#include "Widget/Theme/WidgetThemeEditorLibrary.h"

FString UWidgetEditorToolset::GetBlueprintGraphData(UBlueprint* Blueprint)
{
	if (!Blueprint)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("A source Blueprint is required."));
		return FString();
	}
	return UWidgetBlueprintEditorLibrary::ExportGraphData(Blueprint);
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
