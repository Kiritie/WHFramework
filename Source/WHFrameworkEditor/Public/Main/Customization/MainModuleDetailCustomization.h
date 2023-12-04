// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/Customization/DetailCustomizationBase.h"

class FMainModuleDetailCustomization : public FDetailCustomizationBase
{
public:
	FMainModuleDetailCustomization();

public:
	static TSharedRef<IDetailCustomization> MakeInstance();

	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailLayout ) override;
	
protected:
	FReply OnClickOpenModuleEditorButton();
};
