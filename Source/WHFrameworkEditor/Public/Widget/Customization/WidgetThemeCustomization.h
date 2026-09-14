// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/Customization/ClassCustomizationBase.h"

class FWidgetThemeCustomization : public FClassCustomizationBase
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder) override;

private:
	FReply OnSortStyles();

	IDetailLayoutBuilder* DetailBuilder;
};
