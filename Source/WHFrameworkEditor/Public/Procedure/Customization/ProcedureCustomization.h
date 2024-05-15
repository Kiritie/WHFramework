// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/ClassCustomizationBase.h"

class FProcedureCustomization : public FClassCustomizationBase
{
public:
	FProcedureCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;

protected:
	FReply OnClickGetCameraViewButton();
	
	FReply OnClickPasteCameraViewButton();
};
