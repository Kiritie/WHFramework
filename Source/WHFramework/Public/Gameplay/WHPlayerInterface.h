// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Character/CharacterModuleTypes.h"
#include "UObject/Interface.h"
#include "WHPlayerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UWHPlayerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
* 
*/
class WHFRAMEWORK_API IWHPlayerInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintNativeEvent)
	void Turn(float InValue);
	
	UFUNCTION(BlueprintNativeEvent)
	void MoveH(float InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveV(float InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveForward(float InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveRight(float InValue);

	UFUNCTION(BlueprintNativeEvent)
	void MoveUp(float InValue);

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	class UCameraComponent* GetCameraComp();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	class USpringArmComponent* GetCameraBoom();
};
