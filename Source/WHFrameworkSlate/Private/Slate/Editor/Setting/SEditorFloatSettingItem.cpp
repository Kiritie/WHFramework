#include "Slate/Editor/Setting/SEditorFloatSettingItem.h"

#include "WHFrameworkSlateStyle.h"
#include "Kismet/KismetTextLibrary.h"
#include "Widgets/Input/SEditableTextBox.h"

SEditorFloatSettingItem::SEditorFloatSettingItem()
{
}

void SEditorFloatSettingItem::Construct(const FArguments& InArgs)
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
				return UKismetTextLibrary::Conv_DoubleToText(GetSettingValue().GetFloatValue(), ERoundingMode::HalfToEven, false, false, 1, 324, 0, 2);
			})
			.OnTextCommitted_Lambda([this](const FText& Val, ETextCommit::Type FloatCommitType)
			{
				SetSettingValue(FCString::Atof(*Val.ToString()));
			})
		]
	);
}

void SEditorFloatSettingItem::OnRefresh()
{
}
