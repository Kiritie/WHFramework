// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SceneObjectInterface.generated.h"

class ISceneContainerInterface;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USceneObjectInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API ISceneObjectInterface
{
	GENERATED_BODY()

public:
	// virtual void OnSpawn(ISceneContainerInterface* InContainer) = 0;
	//
	// virtual void OnDestroy() = 0;

public:
	virtual void RemoveFromContainer() { }

public:
	virtual USceneComponent* GetScenePoint() const { return nullptr; }

	virtual ISceneContainerInterface* GetContainer() const { return nullptr; }
	virtual void SetContainer(ISceneContainerInterface* InContainer) { };

	// virtual FTransform GetBirthPos() const = 0;
	// virtual void SetBirthPos(FTransform InBirthPos) = 0;

	// virtual void OnChangeBirthPos(FTransform InBirthPos) = 0;
	// virtual void OnRep_BirthPos() = 0;

	// virtual bool IsVisible() const = 0;
	// virtual void SetVisible(bool bInVisible) = 0;

	// virtual void OnChangeVisible(bool bInVisible) = 0;
	// virtual void OnRep_Visible() = 0;
};
