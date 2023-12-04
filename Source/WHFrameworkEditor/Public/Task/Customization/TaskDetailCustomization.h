// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Common/Customization/DetailCustomizationBase.h"

class FTaskDetailCustomization : public FDetailCustomizationBase
{
public:
	FTaskDetailCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;
};
