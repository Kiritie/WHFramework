// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/DetailCustomizationBase.h"

class FProcedureDetailCustomization : public FDetailCustomizationBase
{
public:
	FProcedureDetailCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:
	FReply OnClickGetCameraViewButton();
	
	FReply OnClickPasteCameraViewButton();
};
