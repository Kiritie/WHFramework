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

bool UWidgetThemeEditorLibrary::SetProjectDefault(UWidgetTheme* InTheme, FString& OutError)
{
	OutError.Reset();
	return FWidgetThemeEditorService::SetProjectDefault(InTheme, OutError);
}
