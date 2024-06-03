#include "Slate/Editor/Setting/SEditorBoolSettingItem.h"

SEditorBoolSettingItem::SEditorBoolSettingItem()
{
}

void SEditorBoolSettingItem::Construct(const FArguments& InArgs)
{
	SEditorSettingItemBase::Construct(
		SEditorSettingItemBase::FArguments()
		.Padding(InArgs._Padding)
		.SettingItem(InArgs._SettingItem)
		.OnValueChanged(InArgs._OnValueChanged)
		.DescContent()
		[
			SNew(SCheckBox)
			// .Style(&FWHFrameworkSlateStyle::Get().GetWidgetStyle<FCheckBoxStyle>("CheckBoxes.Tab.Default"))
			.Padding(FMargin(0.f))
			.IsChecked_Lambda([this]()
			{
				return GetSettingValue().GetBooleanValue() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState State)
			{
				SetSettingValue(State == ECheckBoxState::Checked);
			})
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(5.f, 0.f, 0.f, 0.f))
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.Text(InArgs._SettingItem.ItemValue.GetDescription())
					.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
				]
			]
		]
	);
}

void SEditorBoolSettingItem::OnRefresh()
{
}
