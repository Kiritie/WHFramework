#include "Slate/Editor/Setting/SEditorTextSettingItem.h"
#include "Widgets/Input/SEditableTextBox.h"

SEditorTextSettingItem::SEditorTextSettingItem()
{
}

void SEditorTextSettingItem::Construct(const FArguments& InArgs)
{
	SEditorSettingItemBase::Construct(
		SEditorSettingItemBase::FArguments()
		.Padding(InArgs._Padding)
		.SettingItem(InArgs._SettingItem)
		.OnValueChanged(InArgs._OnValueChanged)
		.Content()
		[
			SNew(SEditableTextBox)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			.Text_Lambda([this]()
			{
				return FText::FromString(GetSettingValue().GetStringValue());
			})
			.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type TextCommitType)
			{
				SetSettingValue(Val.ToString());
			})
		]
	);
}

void SEditorTextSettingItem::OnRefresh()
{
}
