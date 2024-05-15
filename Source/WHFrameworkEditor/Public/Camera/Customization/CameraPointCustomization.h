// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/ClassCustomizationBase.h"

class FCameraPointCustomization : public FClassCustomizationBase
{
public:
	FCameraPointCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:
	FReply OnClickGetCameraViewButton();
	
	FReply OnClickPasteCameraViewButton();
	
	FReply OnClickSwitchCameraPointButton();
};
