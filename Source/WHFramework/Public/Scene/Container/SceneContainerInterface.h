// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Scene/Object/SceneObjectInterface.h"
#include "UObject/Interface.h"
#include "SceneContainerInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USceneContainerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API ISceneContainerInterface
{
	GENERATED_BODY()

public:
	virtual void SpawnSceneObject(ISceneObjectInterface* InSceneObject) = 0;

	virtual void AddSceneObject(ISceneObjectInterface* InSceneObject) = 0;

	virtual void RemoveSceneObject(ISceneObjectInterface* InSceneObject) = 0;

	virtual void DestroySceneObject(ISceneObjectInterface* InSceneObject) = 0;
};
