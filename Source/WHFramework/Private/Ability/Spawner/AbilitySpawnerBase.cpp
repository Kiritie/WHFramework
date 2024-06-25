// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Spawner/AbilitySpawnerBase.h"

#include "Ability/AbilityModuleNetworkComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"
#include "Main/MainModule.h"
#include "Net/UnrealNetwork.h"

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
#endif
}

void AAbilitySpawnerBase::BeginPlay()
{
	Super::BeginPlay();

	if(bAutoSpawn)
	{
		Spawn();
	}
}

void AAbilitySpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if(bAutoDestroy)
	{
		Destroy();
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
