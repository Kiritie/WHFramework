#include "Slate/Editor/Setting/SEditorSettingItemBase.h"

#include "Slate/Editor/Misc/SEditorSplitLine.h"

SEditorSettingItemBase::SEditorSettingItemBase()
{
}

void SEditorSettingItemBase::Construct(const FArguments& InArgs)
{
	OnValueChanged = InArgs._OnValueChanged;
	SettingName = InArgs._SettingItem.ItemName;
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.f))
		.Padding(InArgs._Padding)
		[
			SNew(SVerticalBox)

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			.AutoHeight()
			[
				InArgs._DescContent.Widget != SNullWidget::NullWidget ? InArgs._DescContent.Widget : 
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
					.Text(InArgs._SettingItem.ItemValue.GetDescription())
					.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			]

			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0.f, 0.f, 0.f, 10.f))
			.AutoHeight()
			[
				InArgs._Content.Widget != SNullWidget::NullWidget ? InArgs._Content.Widget : 
					SNew(SSpacer)
					.Visibility(EVisibility::Collapsed)
			]
		]
	];

	SetSettingValue(InArgs._SettingItem.ItemValue);
}

void SEditorSettingItemBase::OnRefresh()
{
}

void SEditorSettingItemBase::Refresh()
{
}

FName SEditorSettingItemBase::GetSettingName() const
{
	return SettingName;
}

FParameter SEditorSettingItemBase::GetSettingValue() const
{
	return SettingValue;
}

void SEditorSettingItemBase::SetSettingValue(const FParameter& InValue)
{
	SettingValue = InValue;
	if(OnValueChanged.IsBound())
	{
		OnValueChanged.Execute(SettingName, SettingValue);
	}
	Refresh();
}
