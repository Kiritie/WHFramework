// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/DetailCustomizationBase.h"

class FFSMComponentDetailCustomization : public FDetailCustomizationBase
{
public:
	FFSMComponentDetailCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;
};