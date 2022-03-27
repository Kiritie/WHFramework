﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "PhysicsVolumeTypes.h"
#include "GameFramework/PhysicsVolume.h"
#include "Scene/Object/SceneObjectInterface.h"
#include "SpawnPool/SpawnPoolInterface.h"

#include "PhysicsVolumeBase.generated.h"

UCLASS()
class WHFRAMEWORK_API APhysicsVolumeBase : public APhysicsVolume, public ISceneObjectInterface, public ISpawnPoolInterface
{
	GENERATED_BODY()
	
public:	
	APhysicsVolumeBase();

	//////////////////////////////////////////////////////////////////////////
    /// Physics Volume
protected:
	UPROPERTY(VisibleAnywhere, Category = "PhysicsVolume")
	FName VolumeName;

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaSeconds) override;

	virtual void Initialize(const FPhysicsVolumeData& InPhysicsVolumeData);

	virtual void OnSpawn_Implementation() override;

	virtual void OnDespawn_Implementation() override;
};
