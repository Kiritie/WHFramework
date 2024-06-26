// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

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
	FGuid GetActorID() const;
	
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	void SetActorID(const FString& InID);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	TScriptInterface<ISceneContainerInterface> GetContainer() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	void SetContainer(const TScriptInterface<ISceneContainerInterface>& InContainer);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	bool IsVisible() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SceneActor")
	void SetActorVisible(bool bInVisible);
};
