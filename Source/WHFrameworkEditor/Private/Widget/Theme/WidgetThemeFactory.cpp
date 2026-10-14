#include "Widget/Theme/WidgetThemeFactory.h"

#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Widget/Theme/WidgetTheme.h"

UWidgetThemeFactory::UWidgetThemeFactory()
{
	SupportedClass = UWidgetTheme::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

uint32 UWidgetThemeFactory::GetMenuCategories() const
{
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().FindAdvancedAssetCategory(TEXT("WHFramework"));
}

FText UWidgetThemeFactory::GetDisplayName() const
{
	return NSLOCTEXT("WidgetTheme", "ThemeAsset", "主题资产");
}

UObject* UWidgetThemeFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UWidgetTheme* Theme = NewObject<UWidgetTheme>(InParent, Class, Name, Flags | RF_Transactional);
	Theme->bRuntimeTheme = false;
	return Theme;
}
