// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SceneActorInterface.generated.h"

class ISceneContainerInterface;
// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class USceneActorInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class WHFRAMEWORK_API ISceneActorInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	void RemoveFromContainer();

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	FName GetObjectName() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	void SetObjectName(FName InName);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	AActor* GetContainer() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	void SetContainer(AActor* InContainer);
};
