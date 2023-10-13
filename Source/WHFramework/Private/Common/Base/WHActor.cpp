// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Base/WHActor.h"

#include "Scene/SceneModuleBPLibrary.h"
#include "Scene/Container/SceneContainerInterface.h"

AWHActor::AWHActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;
}

void AWHActor::OnInitialize_Implementation()
{
	
}

void AWHActor::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void AWHActor::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHActor::OnTermination_Implementation(EPhase InPhase)
{
	
}

void AWHActor::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0) && InParams[0].GetParameterType() == EParameterType::Vector)
	{
		SetActorLocation(InParams[0].GetVectorValue());
	}

	if(InParams.IsValidIndex(1) && InParams[1].GetParameterType() == EParameterType::Rotator)
	{
		SetActorRotation(InParams[1].GetRotatorValue());
	}

	if(InParams.IsValidIndex(2) && InParams[2].GetParameterType() == EParameterType::Vector)
	{
		SetActorScale3D(InParams[2].GetVectorValue());
	}

	Execute_SetActorVisible(this, true);
	
	USceneModuleBPLibrary::AddSceneActor(this);
}

void AWHActor::OnDespawn_Implementation(bool bRecovery)
{
	Execute_SetActorVisible(this, false);

	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	
	USceneModuleBPLibrary::RemoveSceneActor(this);
	if(Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
}

void AWHActor::OnReset_Implementation()
{
	
}

void AWHActor::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for(auto Iter : AttachedActors)
	{
		ISceneActorInterface::Execute_SetActorVisible(Iter, bInVisible);
	}
}

void AWHActor::BeginPlay()
{
	Super::BeginPlay();

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::None);
	}
}

void AWHActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::None);
	}
}

void AWHActor::Tick(float DeltaSeconds)
{
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
