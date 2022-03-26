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
	virtual void Initialize(FTransform InBirthPos) = 0;

	virtual USceneComponent* GetScenePoint() const = 0;

	virtual FTransform GetBirthPos() const = 0;

	virtual void OnChangeBirthPos(FTransform InBirthPos) = 0;
	virtual void OnRep_BirthPos() = 0;

	virtual bool GetDisplay() const = 0;
	virtual void SetDisplay(bool bInDisplay) = 0;

	virtual void OnChangeDisplay(bool bInDisplay) = 0;
	virtual void OnRep_Display() = 0;
};
