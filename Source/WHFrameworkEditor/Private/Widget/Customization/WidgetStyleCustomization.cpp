// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Customization/WidgetStyleCustomization.h"

#include "CommonButtonBase.h"
#include "CommonBorder.h"
#include "CommonRichTextBlock.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Components/TextWidgetTypes.h"
#include "Components/RichTextBlock.h"
#include "Components/ScrollBox.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "Widget/Style/CommonButtonStyleN.h"

#define LOCTEXT_NAMESPACE "WidgetStyleCustomization"

FWidgetStyleCustomization::FWidgetStyleCustomization(TArray<FWidgetStyleHiddenProperty> InProperties)
	: Properties(MoveTemp(InProperties))
{
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeButtonInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({{TEXT("Style"), UCommonButtonBase::StaticClass()}}));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeTextInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({
		{TEXT("Style"), UCommonTextBlock::StaticClass()},
		{TEXT("ScrollStyle"), UCommonTextBlock::StaticClass()},
		{TEXT("Font"), UTextBlock::StaticClass()},
		{TEXT("StrikeBrush"), UTextBlock::StaticClass()},
		{TEXT("ColorAndOpacity"), UTextBlock::StaticClass()},
		{TEXT("ShadowOffset"), UTextBlock::StaticClass()},
		{TEXT("ShadowColorAndOpacity"), UTextBlock::StaticClass()},
		{TEXT("Margin"), UTextLayoutWidget::StaticClass()},
		{TEXT("LineHeightPercentage"), UTextLayoutWidget::StaticClass()},
		{TEXT("ApplyLineHeightToBottomLine"), UTextLayoutWidget::StaticClass()},
	}));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeImageInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({{TEXT("Brush"), UImage::StaticClass()}}));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeBorderInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({
		{TEXT("Style"), UCommonBorder::StaticClass()},
		{TEXT("Background"), UBorder::StaticClass()},
	}));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeRichTextInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({
		{TEXT("DefaultTextStyleOverrideClass"), UCommonRichTextBlock::StaticClass()},
		{TEXT("ScrollStyle"), UCommonRichTextBlock::StaticClass()},
		{TEXT("DefaultTextStyleOverride"), URichTextBlock::StaticClass()},
		{TEXT("bOverrideDefaultStyle"), URichTextBlock::StaticClass()},
	}));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeScrollInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({
		{TEXT("WidgetStyle"), UScrollBox::StaticClass()},
		{TEXT("WidgetBarStyle"), UScrollBox::StaticClass()},
		{TEXT("ScrollbarThickness"), UScrollBox::StaticClass()},
	}));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeButtonStyleInstance()
{
	return MakeShareable(new FWidgetStyleCustomization({
		{TEXT("NormalTextStyle"), UCommonButtonStyle::StaticClass()},
		{TEXT("NormalHoveredTextStyle"), UCommonButtonStyle::StaticClass()},
		{TEXT("SelectedTextStyle"), UCommonButtonStyle::StaticClass()},
		{TEXT("SelectedHoveredTextStyle"), UCommonButtonStyle::StaticClass()},
		{TEXT("DisabledTextStyle"), UCommonButtonStyle::StaticClass()},
	}));
}

void FWidgetStyleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	for (const FWidgetStyleHiddenProperty& Property : Properties)
	{
		DetailLayoutBuilder.HideProperty(Property.Name, Property.OwnerClass);
	}
	UCommonButtonStyleN* ButtonStyle = SelectedObjectsList.Num() == 1 ? Cast<UCommonButtonStyleN>(SelectedObjectsList[0].Get()) : nullptr;
	if (!ButtonStyle)
	{
		return;
	}
	TSharedRef<IPropertyHandle> IconOnlyProperty = DetailLayoutBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UCommonButtonStyleN, bIconOnly));
	TWeakPtr<IPropertyUtilities> PropertyUtilities = DetailLayoutBuilder.GetPropertyUtilities();
	IconOnlyProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([PropertyUtilities]()
	{
		if (TSharedPtr<IPropertyUtilities> Utilities = PropertyUtilities.Pin())
		{
			Utilities->RequestForceRefresh();
		}
	}));
	if (!ButtonStyle->bIconOnly)
	{
		return;
	}
	DetailLayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, bSingleMaterial), UCommonButtonStyle::StaticClass());
	DetailLayoutBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, SingleMaterialBrush), UCommonButtonStyle::StaticClass());
	const auto AddIconColor = [&DetailLayoutBuilder](FName InProperty, FName InCategory, const FText& InLabel)
	{
		TSharedRef<IPropertyHandle> BrushProperty = DetailLayoutBuilder.GetProperty(InProperty, UCommonButtonStyle::StaticClass());
		TSharedPtr<IPropertyHandle> TintProperty = BrushProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSlateBrush, TintColor));
		DetailLayoutBuilder.HideProperty(BrushProperty);
		DetailLayoutBuilder.EditCategory(InCategory).AddProperty(TintProperty)
			.DisplayName(InLabel)
			.ToolTip(LOCTEXT("IconStateTint", "Tint applied to the icon in this button state."));
	};
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, NormalBase), TEXT("Normal"), LOCTEXT("NormalBaseColor", "Base Color"));
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, NormalHovered), TEXT("Normal"), LOCTEXT("NormalHoveredColor", "Hovered Color"));
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, NormalPressed), TEXT("Normal"), LOCTEXT("NormalPressedColor", "Pressed Color"));
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, SelectedBase), TEXT("Selected"), LOCTEXT("SelectedBaseColor", "Base Color"));
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, SelectedHovered), TEXT("Selected"), LOCTEXT("SelectedHoveredColor", "Hovered Color"));
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, SelectedPressed), TEXT("Selected"), LOCTEXT("SelectedPressedColor", "Pressed Color"));
	AddIconColor(GET_MEMBER_NAME_CHECKED(UCommonButtonStyle, Disabled), TEXT("Disabled"), LOCTEXT("DisabledColor", "Color"));
}

#undef LOCTEXT_NAMESPACE
