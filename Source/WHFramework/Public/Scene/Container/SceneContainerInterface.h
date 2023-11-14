// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
	virtual bool HasSceneActor(const FString& InID, bool bEnsured = true) const = 0;

	virtual AActor* GetSceneActor(const FString& InID, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true) const = 0;

	virtual bool AddSceneActor(AActor* InActor) = 0;

	virtual bool RemoveSceneActor(AActor* InActor) = 0;

	virtual void LoadSceneActors(FSaveData* InSaveData) {}

	virtual void SpawnSceneActors() {}

	virtual void DestroySceneActors() {}
};
