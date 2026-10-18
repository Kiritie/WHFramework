// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/ClassCustomizationBase.h"

struct FWidgetStyleHiddenProperty
{
	FName Name;
	const UStruct* OwnerClass = nullptr;
};

class FWidgetStyleCustomization : public FClassCustomizationBase
{
public:
	static TSharedRef<IDetailCustomization> MakeButtonInstance();

	static TSharedRef<IDetailCustomization> MakeTextInstance();

	static TSharedRef<IDetailCustomization> MakeImageInstance();

	static TSharedRef<IDetailCustomization> MakeBorderInstance();

	static TSharedRef<IDetailCustomization> MakeRichTextInstance();

	static TSharedRef<IDetailCustomization> MakeScrollInstance();

	static TSharedRef<IDetailCustomization> MakeButtonStyleInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder) override;

private:
	explicit FWidgetStyleCustomization(TArray<FWidgetStyleHiddenProperty> InProperties);

private:
	TArray<FWidgetStyleHiddenProperty> Properties;
};
