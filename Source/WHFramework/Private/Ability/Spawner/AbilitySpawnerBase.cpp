// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilitySpawnerBase.h"
#include "Scene/Actor/SceneActorIdentity.h"
#include "Scene/SceneModule.h"
#include "Main/MainModuleStatics.h"
#include "Ability/Inventory/InventoryItemTypes.h"

#include "Ability/Spawner/Widget/WidgetAbilitySpawnerInfo.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Math/MathTypes.h"
#include "Net/UnrealNetwork.h"
#include "Scene/SceneModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Widget/World/WorldWidgetComponent.h"

AAbilitySpawnerBase::AAbilitySpawnerBase()
{
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	bReplicates = true;

	GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(false);

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(FName("Arrow"));
	if(ArrowComponent)
	{
		ArrowComponent->SetupAttachment(GetCapsuleComponent());
		ArrowComponent->ArrowColor = FColor(150, 200, 255);
		ArrowComponent->ArrowSize = 1.0f;
		ArrowComponent->bTreatAsASprite = true;
		ArrowComponent->bIsScreenSizeScaled = true;
	}

	WidgetComponent = CreateEditorOnlyDefaultSubobject<UWorldWidgetComponent>(FName("Widget"));
	if(WidgetComponent)
	{
		WidgetComponent->SetupAttachment(GetCapsuleComponent());
		WidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
		WidgetComponent->SetOrientCamera(true);

		static ConstructorHelpers::FClassFinder<UWidgetAbilitySpawnerInfo> SpawnerInfoClassFinder(TEXT("/Script/UMGEditor.WidgetBlueprint'/WHFramework/Blueprints/Widget/Ability/WBP_SpawnerInfo_Base.WBP_SpawnerInfo_Base_C'"));
		if(SpawnerInfoClassFinder.Succeeded())
		{
			WidgetComponent->SetWorldWidgetClass(SpawnerInfoClassFinder.Class);
		}
	}
#endif

	bInitialized = false;
	bVisible = true;
	Container = nullptr;

	bAutoSpawn = false;
}

void AAbilitySpawnerBase::PostActorCreated()
{
	Super::PostActorCreated();
	SceneActorIdentity::InitializeCreated(*this, ActorID);
}

#if WITH_EDITOR
void AAbilitySpawnerBase::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);
	SceneActorIdentity::InitializeEditorDuplicate(*this, ActorID, DuplicateMode);
}

void AAbilitySpawnerBase::PostEditImport()
{
	Super::PostEditImport();
	SceneActorIdentity::InitializeEditorImport(*this, ActorID);
}
#endif

bool AAbilitySpawnerBase::SetActorID_Implementation(FGuid InID)
{
	if ((Container || bSceneIdentityCommitted) && ActorID != InID) return false;
	return SceneActorIdentity::Assign(*this, ActorID, InID);
}

void AAbilitySpawnerBase::OnRep_ActorID()
{
	SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
}

void AAbilitySpawnerBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if (bSceneRegistrationFailed) return;
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

void AAbilitySpawnerBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
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
	if (HasAuthority())
	{
		ActorID.Invalidate();
		ForceNetUpdate();
	}
}

void AAbilitySpawnerBase::OnInitialize_Implementation()
{
	if (bSceneRegistrationFailed) return;
	bInitialized = true;
	
	Execute_SetActorVisible(this, bVisible);
	if (HasAuthority())
	{
		if (!ActorID.IsValid() || !USceneModuleStatics::AddSceneActor(this, this))
		{
			UE_LOG(LogTemp, Error, TEXT("场景实体初始化失败：%s id=%s"), *GetNameSafe(this), *ActorID.ToString());
			bSceneRegistrationFailed = true;
			bInitialized = false;
			SetActorHiddenInGame(true);
			SetActorEnableCollision(false);
			SetActorTickEnabled(false);
			return;
		}
	}
	else if (ActorID.IsValid() && !USceneModuleStatics::AddSceneActor(this, this))
	{
		SceneActorIdentity::QueueReplication(*this, SceneIdentityRetry);
	}
	bSceneIdentityCommitted = true;
}

void AAbilitySpawnerBase::OnPreparatory_Implementation()
{
#if WITH_EDITORONLY_DATA
	if(WidgetComponent)
	{
		WidgetComponent->SetVisibility(false);
	}
#endif

	if(bAutoSpawn)
	{
		Spawn();
	}
}

void AAbilitySpawnerBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AAbilitySpawnerBase::OnTermination_Implementation()
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld())) Scene->RemoveSceneActor(this);

	Destroy();
}

void AAbilitySpawnerBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	if(WidgetComponent)
	{
		for (UWorldWidgetBase* Widget : WidgetComponent->GetWorldWidgets())
		{
			if(UWidgetAbilitySpawnerInfo* SpawnerInfo = Cast<UWidgetAbilitySpawnerInfo>(Widget)) SpawnerInfo->InitAbilityItem(AbilityItem);
		}
	}
#endif
}

void AAbilitySpawnerBase::SetActorVisible_Implementation(bool bInVisible)
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

void AAbilitySpawnerBase::BeginPlay()
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

void AAbilitySpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SceneActorIdentity::CancelReplication(SceneIdentityRetry);
	if (USceneModule* Scene = UMainModuleStatics::GetModuleForCleanup<USceneModule>(GetWorld()))
	{
		if (EndPlayReason == EEndPlayReason::Destroyed) Scene->MarkSceneActorDestroyed(this);
		Scene->RemoveSceneActor(this);
	}
	Super::EndPlay(EndPlayReason);
	
	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this);
	}
}

void AAbilitySpawnerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnRefresh(this, DeltaSeconds);
	}
}

void AAbilitySpawnerBase::Spawn_Implementation()
{
	if (HasAuthority() && !IsValid(AbilityActor))
	{
		AbilityActor = SpawnImpl(AbilityItem);
	}
}

void AAbilitySpawnerBase::Destroy_Implementation()
{
	if (HasAuthority() && IsValid(AbilityActor))
	{
		DestroyImpl(AbilityActor);
		AbilityActor = nullptr;
	}
}

AActor* AAbilitySpawnerBase::SpawnImpl_Implementation(const FAbilityItem& InAbilityItem)
{
	return nullptr;
}

void AAbilitySpawnerBase::DestroyImpl_Implementation(AActor* InAbilityActor)
{
	if(InAbilityActor)
	{
		InAbilityActor->Destroy();
	}
}

void AAbilitySpawnerBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilitySpawnerBase, ActorID);

	DOREPLIFETIME(AAbilitySpawnerBase, AbilityActor);
}

