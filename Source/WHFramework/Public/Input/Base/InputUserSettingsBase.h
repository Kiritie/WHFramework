// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "InputUserSettingsBase.generated.h"

UCLASS()
class WHFRAMEWORK_API UInputUserSettingsBase : public UEnhancedInputUserSettings
{
	GENERATED_BODY()
	
public:
	virtual void ApplySettings() override;
};