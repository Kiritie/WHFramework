// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilitySpawnerBase.h"

#include "Ability/AbilityModuleNetworkComponent.h"
#include "Ability/Spawner/Widget/WidgetAbilitySpawnerInfo.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "Scene/SceneModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Widget/World/WorldWidgetComponent.h"

AAbilitySpawnerBase::AAbilitySpawnerBase()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	bReplicates = true;

	GetCapsuleComponent()->SetShouldUpdatePhysicsVolume(false);

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	ArrowComponent->SetupAttachment(GetCapsuleComponent());
	ArrowComponent->ArrowColor = FColor(150, 200, 255);
	ArrowComponent->ArrowSize = 1.0f;
	ArrowComponent->bTreatAsASprite = true;
	ArrowComponent->bIsScreenSizeScaled = true;

	WidgetComponent = CreateDefaultSubobject<UWorldWidgetComponent>(TEXT("Widget"));
	WidgetComponent->SetupAttachment(GetCapsuleComponent());
	WidgetComponent->SetRelativeLocation(FVector(0.f, 0.f, 50.f));
	WidgetComponent->SetOrientCamera(true);
	
	static ConstructorHelpers::FClassFinder<UWidgetAbilitySpawnerInfo> SpawnerInfoClassFinder(TEXT("/Script/UMGEditor.WidgetBlueprint'/WHFramework/Ability/Blueprints/Widget/WBP_SpawnerInfo_Base.WBP_SpawnerInfo_Base_C'"));
	if(SpawnerInfoClassFinder.Succeeded())
	{
		WidgetComponent->SetWorldWidgetClass(SpawnerInfoClassFinder.Class);
	}
#endif

	bInitialized = false;

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;

	bAutoSpawn = false;
}

void AAbilitySpawnerBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	USceneModuleStatics::AddSceneActor(this);
}

void AAbilitySpawnerBase::OnDespawn_Implementation(bool bRecovery)
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

void AAbilitySpawnerBase::OnInitialize_Implementation()
{
	bInitialized = true;
	
	Execute_SetActorVisible(this, bVisible);

	USceneModuleStatics::AddSceneActor(this);
}

void AAbilitySpawnerBase::OnPreparatory_Implementation(EPhase InPhase)
{
#if WITH_EDITORONLY_DATA
	WidgetComponent->SetVisibility(false);
#endif

	if(bAutoSpawn)
	{
		Spawn();
	}
}

void AAbilitySpawnerBase::OnRefresh_Implementation(float DeltaSeconds)
{
	
}

void AAbilitySpawnerBase::OnTermination_Implementation(EPhase InPhase)
{
	USceneModuleStatics::RemoveSceneActor(this);

	Destroy();
}

void AAbilitySpawnerBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	if(UWidgetAbilitySpawnerInfo* SpawnerInfo = Cast<UWidgetAbilitySpawnerInfo>(WidgetComponent->GetWorldWidget()))
	{
		SpawnerInfo->InitAbilityItem(AbilityItem);
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
		Execute_OnPreparatory(this, EPhase::All);
	}
}

void AAbilitySpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	if(Execute_IsUseDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::All);
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
	if(HasAuthority())
	{
		AbilityActor = SpawnImpl(AbilityItem);
	}
	else if(UAbilityModuleNetworkComponent* AbilityModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAbilityModuleNetworkComponent>())
	{
		AbilityModuleNetworkComponent->ServerSpawnerSpawn(this);
	}
}

void AAbilitySpawnerBase::Destroy_Implementation()
{
	if(HasAuthority())
	{
		DestroyImpl(AbilityActor);
	}
	else if(UAbilityModuleNetworkComponent* AbilityModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UAbilityModuleNetworkComponent>())
	{
		AbilityModuleNetworkComponent->ServerSpawnerDestroy(this);
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

	DOREPLIFETIME(AAbilitySpawnerBase, AbilityActor);
}
