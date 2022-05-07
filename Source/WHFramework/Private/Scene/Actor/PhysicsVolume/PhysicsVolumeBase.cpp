// Fill out your copyright notice in the Description page of Project Settings.


#include "Scene/Actor/PhysicsVolume/PhysicsVolumeBase.h"

#include "TimerManager.h"
#include "Engine/TargetPoint.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

APhysicsVolumeBase::APhysicsVolumeBase()
{
	VolumeName = NAME_None;
}

void APhysicsVolumeBase::BeginPlay()
{
	Super::BeginPlay();

}

void APhysicsVolumeBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

}

void APhysicsVolumeBase::Initialize(const FPhysicsVolumeData& InPhysicsVolumeData)
{
	VolumeName = InPhysicsVolumeData.Name;
	Priority = InPhysicsVolumeData.Priority;
	FluidFriction = InPhysicsVolumeData.FluidFriction;
	TerminalVelocity = InPhysicsVolumeData.TerminalVelocity;
	bWaterVolume = InPhysicsVolumeData.bWaterVolume;
	bPhysicsOnContact = InPhysicsVolumeData.bPhysicsOnContact;
}

void APhysicsVolumeBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
}

void APhysicsVolumeBase::OnDespawn_Implementation()
{
	VolumeName = NAME_None;
}
