// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Base/WHActor.h"

#include "Scene/SceneModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"

AWHActor::AWHActor()
{
	InitializeDefaults();
}

AWHActor::AWHActor(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	InitializeDefaults();
}

void AWHActor::InitializeDefaults()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ActorID = FGuid::NewGuid();

	bVisible = true;
	Container = nullptr;
}

void AWHActor::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ActorID = InParams[0];
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
	bInitialized = true;

	Execute_SetActorVisible(this, bVisible);

	USceneModuleStatics::AddSceneActor(this);
}

void AWHActor::OnPreparatory_Implementation(EPhase InPhase)
{
	
}

void AWHActor::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHActor::OnTermination_Implementation(EPhase InPhase)
{
	USceneModuleStatics::RemoveSceneActor(this);
}

void AWHActor::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FSceneActorSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		ActorID = SaveData.ActorID;
		SetActorTransform(SaveData.SpawnTransform);
	}
}

FSaveData* AWHActor::ToData()
{
	static FSceneActorSaveData SaveData;
	SaveData = FSceneActorSaveData();

	SaveData.ActorID = ActorID;
	SaveData.SpawnTransform = GetActorTransform();

	return &SaveData;
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

	if(Execute_IsUseDefaultLifecycle(this))
	{
		if(!Execute_IsInitialized(this))
		{
			Execute_OnInitialize(this);
		}
		Execute_OnPreparatory(this, EPhase::All);
	}
}

void AWHActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::All);
	}
}

void AWHActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}
