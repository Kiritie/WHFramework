// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/Actor.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Scene/Actor/SceneActorInterface.h"
#include "WHActor.generated.h"

/**
 * 
 */
UCLASS(hidecategories = (Tick, Replication, Rendering, Collision, Actor, Input, LOD, Cooking, Hidden, WorldPartition, Hlod))
class WHFRAMEWORK_API AWHActor : public AActor, public ISceneActorInterface, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:	
	AWHActor();

	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Actor
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor")
	FGuid ActorID;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Actor")
	TScriptInterface<ISceneContainerInterface> Container;
	
public:
	virtual FGuid GetActorID_Implementation() const override { return ActorID; }

	virtual void SetActorID_Implementation(FGuid InActorID) override { ActorID = InActorID; }

	virtual TScriptInterface<ISceneContainerInterface> GetContainer_Implementation() const override { return Container; }

	virtual void SetContainer_Implementation(const TScriptInterface<ISceneContainerInterface>& InContainer) override { Container = InContainer; }

	virtual bool IsVisible_Implementation() const override;

	virtual void SetActorVisible_Implementation(bool bVisible) override;
};
