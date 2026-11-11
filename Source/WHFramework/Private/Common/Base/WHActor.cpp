// Fill out your copyright notice in the Description page of Project Settings.

#include "Common/Base/WHActor.h"
#include "WHFrameworkCoreMacros.h"
#include "Scene/Actor/SceneActorIdentity.h"
#include "Scene/SceneModule.h"
#include "Main/MainModuleStatics.h"
#include "Net/UnrealNetwork.h"
#include "Common/LifecycleTypes.h"
#include "Scene/SceneActorSaveTypes.h"

#include "Math/MathTypes.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"

AWHActor::AWHActor()
{
	PrimaryActorTick.bCanEverTick = true;

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

	bVisible = true;
	Container = nullptr;
}

void AWHActor::PostActorCreated()
{
	Super::PostActorCreated();
	SceneActorIdentity::InitializeCreated(*this, ActorID);
}

#if WITH_EDITOR
void AWHActor::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	SceneActorIdentity::InitializeEditorDuplicate(*this, ActorID, DuplicateMode);
}

void AWHActor::PostEditImport()
{
	Super::PostEditImport();
	SceneActorIdentity::InitializeEditorImport(*this, ActorID);
}
#endif

bool AWHActor::SetActorID_Implementation(FGuid InID)
{
	if ((Container || bSceneIdentityCommitted) && ActorID != InID) return false;
	return SceneActorIdentity::Assign(*this, ActorID, InID);
}

void AWHActor::OnRep_ActorID()
{
	SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void AWHActor::OnSpawn_Implementation(const FParameter& InParam)
{
	if (bSceneRegistrationFailed) return;

	Execute_SetActorVisible(this, true);
	const bool bRegistered = ActorID.IsValid() && USceneModuleStatics::AddSceneActor(this, this);
	if (bRegistered) bSceneIdentityCommitted = true;
	if (!bRegistered && HasAuthority() && Execute_IsUseDefaultLifecycle(this))
	{
		bSceneRegistrationFailed = true;
		bInitialized = false;
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Error, TEXT("场景实体登记失败，停止初始化：%s id=%s"), *GetNameSafe(this), *ActorID.ToString());
		return;
	}
	if (!bRegistered && !HasAuthority() && ActorID.IsValid()) SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void AWHActor::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	Execute_SetActorVisible(this, false);

	SetActorLocationAndRotation(UNDER_Vector, FRotator::ZeroRotator);
	
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
	if(Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
	bSceneIdentityCommitted = false;
	if (HasAuthority()) { ActorID.Invalidate(); ForceNetUpdate(); }
}

void AWHActor::OnInitialize_Implementation()
{
	if (bSceneRegistrationFailed) return;
	bInitialized = true;

	Execute_SetActorVisible(this, bVisible);
	const bool bRegistered = ActorID.IsValid() && USceneModuleStatics::AddSceneActor(this, this);
	if (bRegistered) bSceneIdentityCommitted = true;
	if (!bRegistered && HasAuthority() && Execute_IsUseDefaultLifecycle(this))
	{
		bSceneRegistrationFailed = true;
		bInitialized = false;
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
		SetActorTickEnabled(false);
		UE_LOG(LogTemp, Error, TEXT("场景实体登记失败，停止初始化：%s id=%s"), *GetNameSafe(this), *ActorID.ToString());
		return;
	}
	if (!bRegistered && !HasAuthority() && ActorID.IsValid()) SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void AWHActor::OnPreparatory_Implementation()
{
	
}

void AWHActor::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AWHActor::OnTermination_Implementation()
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);
}

void AWHActor::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!InSaveData.GetPtr<FSceneActorSaveData>()) { MarkLoadDataFailed(); return; }
	auto& SaveData = InSaveData.GetRef<FSceneActorSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		if (!SetActorID_Implementation(SaveData.ActorID))
		{
			MarkLoadDataFailed();
			UE_LOG(LogTemp, Error, TEXT("无法恢复场景实体身份：%s id=%s"), *GetNameSafe(this), *SaveData.ActorID.ToString());
			return;
		}
		SetActorTransform(SaveData.SpawnTransform);
	}
}

FParameter AWHActor::ToData()
{
	FSceneActorSaveData SaveData;

	SaveData.ActorID = ActorID;
	SaveData.SpawnTransform = GetActorTransform();

	return FParameter(MoveTemp(SaveData));
}

void AWHActor::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for(auto Iter : AttachedActors)
	{
		Execute_SetActorVisible(Iter, bInVisible);
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
		if (!Execute_IsInitialized(this)) return;
		Execute_OnPreparatory(this);
	}
}

void AWHActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if(EndPlayReason == EEndPlayReason::Destroyed)
	{
		if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->MarkSceneActorDestroyed(this);
	}
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);

	Super::EndPlay(EndPlayReason);
	
	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
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

void AWHActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AWHActor, ActorID);
}
