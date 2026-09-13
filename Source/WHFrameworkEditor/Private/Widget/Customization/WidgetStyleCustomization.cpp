// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widget/Customization/WidgetStyleCustomization.h"

#include "CommonButtonBase.h"
#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "DetailLayoutBuilder.h"

FWidgetStyleCustomization::FWidgetStyleCustomization(FName InPropertyName, const UStruct* InOwnerClass)
	: PropertyName(InPropertyName), OwnerClass(InOwnerClass)
{
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeButtonInstance()
{
	return MakeShareable(new FWidgetStyleCustomization(TEXT("Style"), UCommonButtonBase::StaticClass()));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeTextInstance()
{
	return MakeShareable(new FWidgetStyleCustomization(TEXT("Style"), UCommonTextBlock::StaticClass()));
}

TSharedRef<IDetailCustomization> FWidgetStyleCustomization::MakeImageInstance()
{
	return MakeShareable(new FWidgetStyleCustomization(TEXT("Brush"), UImage::StaticClass()));
}

void FWidgetStyleCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	FClassCustomizationBase::CustomizeDetails(DetailLayoutBuilder);
	DetailLayoutBuilder.HideProperty(PropertyName, OwnerClass);
}
