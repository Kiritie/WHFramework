#include "Setting/Customization/SettingRegistryCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Setting/SettingRegistry.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SettingRegistryCustomization"

TSharedRef<IDetailCustomization> FSettingRegistryCustomization::MakeInstance()
{
	return MakeShared<FSettingRegistryCustomization>();
}

void FSettingRegistryCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	DetailBuilder = &DetailLayoutBuilder;

	const TSharedRef<IPropertyHandle> VersionProperty = DetailLayoutBuilder.GetProperty(FName("GeneratedSnapshotVersion"), USettingRegistry::StaticClass());
	const TSharedRef<IPropertyHandle> DefinitionsProperty = DetailLayoutBuilder.GetProperty(FName("FinalDefinitions"), USettingRegistry::StaticClass());
	const TSharedRef<IPropertyHandle> PagesProperty = DetailLayoutBuilder.GetProperty(FName("Pages"), USettingRegistry::StaticClass());
	const TSharedRef<IPropertyHandle> OverridesProperty = DetailLayoutBuilder.GetProperty(FName("Overrides"), USettingRegistry::StaticClass());
	DetailLayoutBuilder.HideProperty(VersionProperty);
	DetailLayoutBuilder.HideProperty(DefinitionsProperty);
	DetailLayoutBuilder.HideProperty(PagesProperty);
	DetailLayoutBuilder.HideProperty(OverridesProperty);

	IDetailCategoryBuilder& GeneratedCategory = DetailLayoutBuilder.EditCategory(
		"GeneratedSettings",
		LOCTEXT("GeneratedSettings", "Available Settings"),
		ECategoryPriority::Important);
	GeneratedCategory.AddCustomRow(LOCTEXT("GenerateSnapshot", "Generate Snapshot"))
	.WholeRowContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 8.f, 0.f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Generate", "Generate"))
			.ToolTipText(LOCTEXT("GenerateTooltip", "Regenerate settings from reflected SaveData and configured providers."))
			.OnClicked(this, &FSettingRegistryCustomization::OnGenerateSnapshot)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			VersionProperty->CreatePropertyValueWidget()
		]
	];
	GeneratedCategory.AddProperty(DefinitionsProperty);

	IDetailCategoryBuilder& LayoutCategory = DetailLayoutBuilder.EditCategory(
		"Layout",
		LOCTEXT("Layout", "Layout and Overrides"),
		ECategoryPriority::Important);
	LayoutCategory.AddProperty(PagesProperty);
	LayoutCategory.AddProperty(OverridesProperty);
}

FReply FSettingRegistryCustomization::OnGenerateSnapshot()
{
	for(const TWeakObjectPtr<UObject>& SelectedObject : SelectedObjectsList)
	{
		if(USettingRegistry* Registry = Cast<USettingRegistry>(SelectedObject.Get()))
		{
			Registry->GenerateSnapshot();
		}
	}
	if(DetailBuilder)
	{
		DetailBuilder->ForceRefreshDetails();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
