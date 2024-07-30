#include "Slate/Editor/Setting/SEditorIntegerSettingItem.h"

#include "WHFrameworkSlateStyle.h"
#include "Kismet/KismetTextLibrary.h"
#include "Widgets/Input/SEditableTextBox.h"

SEditorIntegerSettingItem::SEditorIntegerSettingItem()
{
}

void SEditorIntegerSettingItem::Construct(const FArguments& InArgs)
{
	SEditorSettingItemBase::Construct(
		SEditorSettingItemBase::FArguments()
		.Padding(InArgs._Padding)
		.SettingItem(InArgs._SettingItem)
		.OnValueChanged(InArgs._OnValueChanged)
		.Content()
		[
			SNew(SEditableTextBox)
			.Style(FWHFrameworkSlateStyle::Get(), "EditableTextBoxes.SettingItem")
			.Text_Lambda([this]()
			{
				return FText::FromString(FString::FromInt(GetSettingValue().GetIntegerValue()));
			})
			.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type IntegerCommitType)
			{
				SetSettingValue(FCString::Atoi(*Val.ToString()));
			})
		]
	);
}

void SEditorIntegerSettingItem::OnRefresh()
{
}
