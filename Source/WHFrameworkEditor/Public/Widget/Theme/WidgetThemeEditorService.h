#pragma once
#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widget/Theme/WidgetStyleCatalog.h"
#include "Widget/Theme/WidgetTheme.h"

class UWidgetTheme;
class UWidgetStyleCatalog;
class UBlueprint;
class WHFRAMEWORKEDITOR_API FWidgetThemeEditorService
{
public:
	static void RegisterAssetLifecycle();
	static void UnregisterAssetLifecycle();
	static UWidgetStyleCatalog* Catalog();
	static UClass* NativeRoot(EWidgetStyleKind Kind);
	static TArray<FGameplayTag> Tags(const UWidgetTheme* Theme);
	static bool Kind(FGameplayTag Tag, EWidgetStyleKind& Out);
	static UBlueprint* Blueprint(UWidgetTheme* Theme, FGameplayTag Tag);
	static bool Validate(UWidgetTheme* Theme, FString& Report);
	static TArray<FString> GetCreationRoots();
	static bool Create(UWidgetTheme* Theme, FGameplayTag Parent, const FString& Name, EWidgetStyleKind Kind, const FString& DestinationRoot,
					   bool bInheritParent, FString& Report);
	static bool Associate(UWidgetTheme* Theme, FGameplayTag Parent, const FString& Name, UWidgetTheme* SourceTheme, FGameplayTag SourceTag, FString& Report);
	static TArray<UWidgetTheme*> Themes();
	static FString DescribeTag(const UWidgetTheme* Theme, FGameplayTag Tag);
	static bool ValidateName(const FString& Name, FString& Report);
	static bool RenameStyle(UWidgetTheme* Theme, FGameplayTag Tag, const FString& Name, FString& Report);
	static bool RemoveBinding(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report);
	static bool CanRemove(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report);
	static bool CompileStyle(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report);
	static bool CopyProperties(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report);
	static bool CanPasteProperties(UWidgetTheme* Theme, FGameplayTag Tag);
	static bool PasteProperties(UWidgetTheme* Theme, FGameplayTag Tag, FString& Report);
	static bool DuplicateThemeStyles(UWidgetTheme* Theme);
	static bool CanDeleteAssets(const TArray<UObject*>& Assets, FString& Report);
	static bool ExportInventory(const FString& Filename, FString& Report);
	static bool PersistReferenceChanges(FString& Report);
	static bool Save(UWidgetTheme* Theme, FString& Report);
	static bool SetProjectDefault(UWidgetTheme* Theme, FString& Report);
	static bool OrganizeAssets(UWidgetTheme* Theme, FString& Report);
	static bool IsBusy();
	static TArray<FProperty*> EditableProperties(UClass* Class);

private:
	static bool bBusy;
	static bool RegisterTag(const FString& Name, FString& Report);
	static bool RenameTag(FGameplayTag OldTag, FGameplayTag NewTag, FString& Report);
};
