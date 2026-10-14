#include "SWidgetThemePicker.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widget/Theme/WidgetTheme.h"

void SWidgetThemePicker::Construct(const FArguments& InArgs)
{
	SelectedTheme = InArgs._SelectedTheme;
	OnThemeSelected = InArgs._OnThemeSelected;
	bAllowProjectDefault = InArgs._AllowProjectDefault;
	ChildSlot[SNew(SComboButton)
				  .OnGetMenuContent(this, &SWidgetThemePicker::BuildMenu)
				  .ButtonContent()[SNew(STextBlock)
									   .Text_Lambda(
										   [this]
										   { return FText::FromString(SelectedTheme.Get() ? SelectedTheme.Get()->GetPathName() : TEXT("Project default")); })]];
}

TSharedRef<SWidget> SWidgetThemePicker::BuildMenu()
{
	FMenuBuilder Menu(true, nullptr);
	if (bAllowProjectDefault)
	{
		Menu.AddMenuEntry(NSLOCTEXT("WidgetTheme", "ProjectDefault", "Project default"), FText(), FSlateIcon(),
						  FUIAction(FExecuteAction::CreateSPLambda(this, [this] { OnThemeSelected.ExecuteIfBound(nullptr); })));
	}
	FARFilter Filter;
	Filter.ClassPaths.Add(UWidgetTheme::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	TArray<FAssetData> Assets;
	FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get().GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& Left, const FAssetData& Right) { return Left.GetObjectPathString() < Right.GetObjectPathString(); });
	for (const FAssetData& Asset : Assets)
	{
		Menu.AddMenuEntry(
			FText::FromName(Asset.AssetName), FText::FromString(Asset.GetObjectPathString()), FSlateIcon(),
			FUIAction(FExecuteAction::CreateSPLambda(this, [this, Asset] { OnThemeSelected.ExecuteIfBound(Cast<UWidgetTheme>(Asset.GetAsset())); })));
	}
	return Menu.MakeWidget();
}
