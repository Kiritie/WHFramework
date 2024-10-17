// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Base/WHActor.h"

#include "Scene/SceneModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"

AWHActor::AWHActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;
}

void AWHActor::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0) && InParams[0].GetParameterType() == EParameterType::Vector)
	{
		SetActorLocation(InParams[0]);
	}

	if(InParams.IsValidIndex(1) && InParams[1].GetParameterType() == EParameterType::Rotator)
	{
		SetActorRotation(InParams[1]);
	}

	if(InParams.IsValidIndex(2) && InParams[2].GetParameterType() == EParameterType::Vector)
	{
		SetActorScale3D(InParams[2]);
	}

	Execute_SetActorVisible(this, true);
	
	USceneModuleStatics::AddSceneActor(this);
}

void AWHActor::OnDespawn_Implementation(bool bRecovery)
{
	Execute_SetActorVisible(this, false);

	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);
	
	USceneModuleStatics::RemoveSceneActor(this);
	if(Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
}

void AWHActor::OnInitialize_Implementation()
{
	Execute_SetActorVisible(this, bVisible);
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

void AWHActor::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FSceneActorSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData.SpawnTransform);
	}
}

FSaveData* AWHActor::ToData()
{
	return nullptr;
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
		Execute_OnPreparatory(this, EPhase::All);
	}
}

void AWHActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::All);
	}
}

void AWHActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
