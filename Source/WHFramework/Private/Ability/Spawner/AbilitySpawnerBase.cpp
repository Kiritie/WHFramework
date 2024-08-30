// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilitySpawnerBase.h"

#include "Ability/AbilityModuleNetworkComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TextRenderComponent.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"
#include "Scene/SceneModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Engine/Font.h"

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

	TextComponent = CreateEditorOnlyDefaultSubobject<UTextRenderComponent>(TEXT("Text"));
	TextComponent->SetupAttachment(GetCapsuleComponent());
	
	static ConstructorHelpers::FObjectFinder<UFont> RobotoFontObj(TEXT("/Engine/EngineFonts/Roboto"));
	TextComponent->SetFont(RobotoFontObj.Object);
#endif

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;
}

void AAbilitySpawnerBase::OnInitialize_Implementation()
{
	Execute_SetActorVisible(this, bVisible);
}

void AAbilitySpawnerBase::OnPreparatory_Implementation(EPhase InPhase)
{
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
	if(bAutoDestroy)
	{
		Destroy();
	}
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

	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnInitialize(this);
		Execute_OnPreparatory(this, EPhase::All);
	}
}

void AAbilitySpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	
	if(Execute_IsDefaultLifecycle(this))
	{
		Execute_OnTermination(this, EPhase::All);
	}
}

void AAbilitySpawnerBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if(Execute_IsDefaultLifecycle(this))
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
