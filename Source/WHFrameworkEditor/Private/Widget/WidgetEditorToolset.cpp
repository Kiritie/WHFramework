#include "Widget/WidgetEditorToolset.h"

#include "GameplayTagsManager.h"
#include "Kismet/KismetSystemLibrary.h"
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
