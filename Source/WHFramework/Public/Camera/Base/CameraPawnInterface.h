// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Character/CharacterModuleTypes.h"
#include "UObject/Interface.h"
#include "CameraPawnInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCameraPawnInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API ICameraPawnInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	class UCameraComponent* GetCameraComp();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	class USpringArmComponent* GetCameraBoom();
};
