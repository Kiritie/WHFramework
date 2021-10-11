// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SceneObject.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USceneObject : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API ISceneObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void Initialize(FTransform InBirthPos);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	class USceneComponent* GetScenePoint() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	FTransform GetBirthPos() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnChangeBirthPos(FTransform InBirthPos);
	virtual void OnRep_BirthPos() { }

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool GetDisplay() const;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetDisplay(bool bInDisplay);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnChangeDisplay(bool bInDisplay);
	virtual void OnRep_Display() { }
};
