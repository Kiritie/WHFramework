#include "Widget/Theme/WidgetThemeEditorLibrary.h"

#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "Widget/Theme/WidgetThemeFactory.h"
#include "WidgetStylePropertyEditing.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/UnrealType.h"
#include "Widget/Theme/WidgetThemeEditorService.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Framework/Application/SlateApplication.h"

UWidgetTheme* UWidgetThemeEditorLibrary::CreateTheme(const FString& InAssetPath, UWidgetTheme* InParentTheme, bool bRuntimeTheme, FString& OutError)
{
	OutError.Reset();
	if (!FPackageName::IsValidLongPackageName(InAssetPath) || FPackageName::DoesPackageExist(InAssetPath))
	{
		OutError = TEXT("Theme destination is invalid or already exists.");
		return nullptr;
	}
	UWidgetThemeFactory* Factory = NewObject<UWidgetThemeFactory>();
	UWidgetTheme* Theme = Cast<UWidgetTheme>(FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateAsset(
		FPackageName::GetShortName(InAssetPath), FPackageName::GetLongPackagePath(InAssetPath), UWidgetTheme::StaticClass(), Factory));
	if (!Theme)
	{
		OutError = TEXT("Theme factory failed.");
		return nullptr;
	}
	Theme->ParentTheme = InParentTheme;
	Theme->bRuntimeTheme = bRuntimeTheme;
	Theme->MarkPackageDirty();
	return Theme;
}

UBlueprint* UWidgetThemeEditorLibrary::CreateStyle(UWidgetTheme* InTheme, const FString& InTag, UClass* InParentClass, const FString& InDestinationRoot, FString& OutError)
{
	OutError.Reset();
	return FWidgetThemeEditorService::CreateForTag(InTheme, InTag, InParentClass, InDestinationRoot, OutError);
}

bool UWidgetThemeEditorLibrary::SetStyleProperty(UWidgetTheme* InTheme, FGameplayTag InTag, FName InProperty, const FString& InValue, FString& OutError)
{
	OutError.Reset();
	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Managed style properties require a running editor with Slate; use an editor Python script instead of a commandlet.");
		return false;
	}
	const FWidgetStyleBinding* Binding = InTheme ? InTheme->Bindings.Find(InTag) : nullptr;
	UBlueprint* Blueprint = InTheme ? FWidgetThemeEditorService::Blueprint(InTheme, InTag) : nullptr;
	if (!Binding || !Binding->bManaged || !Blueprint || !Blueprint->GeneratedClass)
	{
		OutError = TEXT("Only an owned style can be edited through this theme.");
		return false;
	}
	FProperty* Property = Blueprint->GeneratedClass->FindPropertyByName(InProperty);
	if (!Property || !FWidgetThemeEditorService::EditableProperties(Blueprint->GeneratedClass).Contains(Property))
	{
		OutError = TEXT("The style property is not editable.");
		return false;
	}
	UObject* Defaults = Blueprint->GeneratedClass->GetDefaultObject();
	void* Trial = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
	Property->InitializeValue(Trial);
	Property->CopyCompleteValue(Trial, Property->ContainerPtrToValuePtr<void>(Defaults));
	const bool bImported = Property->ImportText_Direct(*InValue, Trial, Defaults, PPF_None) != nullptr;
	bool bApplied = false;
	if (bImported)
	{
		FString CanonicalValue;
		// Identical delta pointers force complete nested values, including zero enums and null resources.
		// A null delta omits them, so the property editor would merge stale values into the new struct.
		Property->ExportText_Direct(CanonicalValue, Trial, Trial, Defaults, PPF_None);
		FWidgetStylePropertyEditing Properties(Defaults);
		TSharedPtr<IPropertyHandle> Handle = Properties.Find(Property);
		if (Handle)
		{
			FScopedTransaction Transaction(NSLOCTEXT("WidgetTheme", "SetStyleProperty", "Edit managed style property"));
			bApplied = Handle->SetValueFromFormattedString(CanonicalValue) == FPropertyAccess::Success;
			if (bApplied)
			{
				FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
				FBlueprintEditorUtils::PostEditChangeBlueprintActors(Blueprint);
				Blueprint->MarkPackageDirty();
				FWidgetThemeResolver::NotifyThemeChanged();
			}
			else
			{
				Transaction.Cancel();
			}
		}
	}
	Property->DestroyValue(Trial);
	FMemory::Free(Trial);
	if (!bApplied)
	{
		OutError = TEXT("Style property text could not be applied through the property editor.");
	}
	return bApplied;
}

bool UWidgetThemeEditorLibrary::SaveTheme(UWidgetTheme* InTheme, FString& OutError)
{
	OutError.Reset();
	return FWidgetThemeEditorService::Save(InTheme, OutError);
}

bool UWidgetThemeEditorLibrary::RemoveStyleBinding(UWidgetTheme* InTheme, const FString& InTag, FString& OutError)
{
	OutError.Reset();
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*InTag), false);
	if (!InTheme || !Tag.IsValid())
	{
		OutError = TEXT("A theme and a registered style Tag are required.");
		return false;
	}
	return FWidgetThemeEditorService::RemoveBinding(InTheme, Tag, OutError);
}

bool UWidgetThemeEditorLibrary::RemoveDuplicateGlobalTagDefinition(const FString& InTag, const FString& InTargetOwnerPath, FString& OutError)
{
	OutError.Reset();
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*InTag), false);
	if (!Tag.IsValid() || InTargetOwnerPath.IsEmpty())
	{
		OutError = TEXT("A registered Tag and its owning module path are required.");
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Global duplicate Tag cleanup requires a running Unreal Editor with Slate.");
		return false;
	}
	return FWidgetThemeEditorService::RemoveDuplicateGlobalTagDefinition(Tag, InTargetOwnerPath, OutError);
}

bool UWidgetThemeEditorLibrary::MigrateTag(const FString& InOldTag, const FString& InNewTag, FString& OutError)
{
	OutError.Reset();
	const FGameplayTag OldTag = FGameplayTag::RequestGameplayTag(FName(*InOldTag), false);
	const FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*InNewTag), false);
	if (!OldTag.IsValid() || !NewTag.IsValid())
	{
		OutError = TEXT("Both the old and replacement Gameplay Tags must be registered before migration.");
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Direct Tag migration requires a running Unreal Editor with Slate.");
		return false;
	}
	return FWidgetThemeEditorService::MigrateTag(OldTag, NewTag, OutError);
}

bool UWidgetThemeEditorLibrary::MigrateTagReferences(const FString& InOldTagName, const FString& InNewTag, FString& OutError)
{
	OutError.Reset();
	const FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*InNewTag), false);
	if (!NewTag.IsValid())
	{
		OutError = TEXT("The replacement Gameplay Tag must be registered before references are migrated.");
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Tag reference migration requires a running Unreal Editor with Slate.");
		return false;
	}
	return FWidgetThemeEditorService::MigrateTagReferences(InOldTagName, NewTag, OutError);
}

bool UWidgetThemeEditorLibrary::MigrateTagAlias(const FString& InOldAlias, const FString& InNewTag, FString& OutError)
{
	OutError.Reset();
	const FGameplayTag NewTag = FGameplayTag::RequestGameplayTag(FName(*InNewTag), false);
	const FName OldAlias(*InOldAlias);
	if (OldAlias.IsNone() || !NewTag.IsValid())
	{
		OutError = TEXT("A non-empty old alias and a registered replacement Gameplay Tag are required.");
		return false;
	}
	if (!FSlateApplication::IsInitialized())
	{
		OutError = TEXT("Direct alias migration requires a running Unreal Editor with Slate.");
		return false;
	}
	return FWidgetThemeEditorService::MigrateTagAlias(OldAlias, NewTag, OutError);
}

bool UWidgetThemeEditorLibrary::SetProjectDefault(UWidgetTheme* InTheme, FString& OutError)
{
	OutError.Reset();
	return FWidgetThemeEditorService::SetProjectDefault(InTheme, OutError);
}
