// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/ClassCustomizationBase.h"

class FFSMComponentCustomization : public FClassCustomizationBase
{
public:
	FFSMComponentCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;
};
