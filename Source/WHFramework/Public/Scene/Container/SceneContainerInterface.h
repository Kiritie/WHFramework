// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "WHFramework.h"

#include "Scene/Actor/SceneActorInterface.h"
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
	virtual bool HasSceneActor(FGuid InID, bool bEnsured = true) const { return false; }

	virtual AActor* GetSceneActor(FGuid InID, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true) const { return nullptr; }

	virtual void AddSceneActor(AActor* InActor) { }

	virtual void RemoveSceneActor(AActor* InActor) { }
};
