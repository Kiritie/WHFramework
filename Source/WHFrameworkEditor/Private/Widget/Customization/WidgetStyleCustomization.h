// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/ClassCustomizationBase.h"

class FWidgetStyleCustomization : public FClassCustomizationBase
{
public:
	static TSharedRef<IDetailCustomization> MakeButtonInstance();

	static TSharedRef<IDetailCustomization> MakeTextInstance();

	static TSharedRef<IDetailCustomization> MakeImageInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder) override;

private:
	FWidgetStyleCustomization(FName InPropertyName, const UStruct* InOwnerClass);

private:
	FName PropertyName;

	const UStruct* OwnerClass;
};
