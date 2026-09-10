#include "Slate/Editor/Setting/SEditorEnumSettingItem.h"

#include "WHFrameworkSlateStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/STextComboBox.h"

SEditorEnumSettingItem::SEditorEnumSettingItem()
{
}

void SEditorEnumSettingItem::Construct(const FArguments& InArgs)
{
	const FEnumParameterValue EnumValue = InArgs._SettingItem.ItemValue.Get<FEnumParameterValue>();
	for(const FString& Iter : EnumValue.EnumNames)
	{
		EnumNames.Add(MakeShared<FString>(Iter));
	}
	SEditorSettingItemBase::Construct(
		SEditorSettingItemBase::FArguments()
		.Padding(InArgs._Padding)
		.SettingItem(InArgs._SettingItem)
		.OnValueChanged(InArgs._OnValueChanged)
		.Content()
		[
			SNew(SBox)
			.HeightOverride(34.f)
			[
				SNew(STextComboBox)
				.ComboBoxStyle(FWHFrameworkSlateStyle::Get(), "ComboBoxes.SettingItem")
				.OptionsSource(&EnumNames)
				.InitiallySelectedItem(EnumNames[EnumValue.EnumValue])
				.OnSelectionChanged_Lambda([this, EnumValue](TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
				{
					SetSettingValue(FEnumParameterValue(EnumValue.EnumNames, EnumNames.Find(Selection)));
				})
			]
		]
	);
}

void SEditorEnumSettingItem::OnRefresh()
{
}
