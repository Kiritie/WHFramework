// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UserSettings/EnhancedInputUserSettings.h"

#include "PlayerMappableKeyProfileBase.generated.h"

UCLASS()
class WHFRAMEWORK_API UPlayerMappableKeyProfileBase : public UEnhancedPlayerMappableKeyProfile
{
	GENERATED_BODY()

protected:
	virtual void EquipProfile() override;
	
	virtual void UnEquipProfile() override;
};