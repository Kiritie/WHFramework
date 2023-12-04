// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/DetailCustomizationBase.h"

class FCameraModuleDetailCustomization : public FDetailCustomizationBase
{
public:
	FCameraModuleDetailCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:
	FReply OnClickCopyCameraParamsButton();
};
