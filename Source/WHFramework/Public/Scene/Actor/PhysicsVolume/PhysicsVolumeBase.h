// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "PhysicsVolumeTypes.h"
#include "GameFramework/PhysicsVolume.h"
#include "ObjectPool/ObjectPoolInterface.h"
#include "Scene/Actor/SceneActorInterface.h"

#include "PhysicsVolumeBase.generated.h"

UCLASS()
class WHFRAMEWORK_API APhysicsVolumeBase : public APhysicsVolume, public ISceneActorInterface, public IObjectPoolInterface
{
	GENERATED_BODY()
	
public:	
	APhysicsVolumeBase();

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(VisibleAnywhere, Category = "PhysicsVolume")
	FName VolumeName;
public:
	UFUNCTION(BlueprintPure)
	FName GetVolumeName() const { return VolumeName; }

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaSeconds) override;

	virtual void Initialize(const FPhysicsVolumeData& InPhysicsVolumeData);

	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation() override;
};
