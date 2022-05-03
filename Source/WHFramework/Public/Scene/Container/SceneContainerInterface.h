// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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
	virtual bool HasSceneActorByClass(TSubclassOf<AActor> InClass, bool bEnsured = true) const { return false; }

	virtual bool HasSceneActorByName(FName InName, bool bEnsured = true) const { return false; }

	virtual AActor* GetSceneActorByClass(TSubclassOf<AActor> InClass, bool bEnsured = true) const { return nullptr; }

	virtual AActor* GetSceneActorByName(FName InName, TSubclassOf<AActor> InClass = nullptr, bool bEnsured = true) const { return nullptr; }

	virtual void AddSceneActor(AActor* InActor) { }

	virtual void AddSceneActorByName(FName InName, AActor* InActor) { }

	virtual void RemoveSceneActor(AActor* InActor) { }

	virtual void RemoveSceneActorByName(FName InName) { }

	virtual void DestroySceneActor(AActor* InActor) { }

	virtual void DestroySceneActorByName(FName InName) { }
};
