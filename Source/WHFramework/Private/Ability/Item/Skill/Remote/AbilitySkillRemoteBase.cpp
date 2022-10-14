// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/Remote/AbilitySkillRemoteBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Ability/Vitality/AbilityVitalityInterface.h"

// Sets default values
AAbilitySkillRemoteBase::AAbilitySkillRemoteBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	SphereComponent->SetupAttachment(RootComponent);
	SphereComponent->SetCollisionProfileName(TEXT("Skill"));
	SphereComponent->SetSphereRadius(50);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilitySkillRemoteBase::OnBeginOverlap);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);

	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->ProjectileGravityScale = 0;
	MovementComponent->BounceVelocityStopSimulatingThreshold = false;
	MovementComponent->Velocity = FVector::ZeroVector;

	DurationTime = 10;
	InitialVelocity = FVector(3000, 0, 0);
}

void AAbilitySkillRemoteBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(Execute_CanHitTarget(this, OtherActor))
	{
		Execute_OnHitTarget(this, OtherActor, SweepResult);
	}
}

void AAbilitySkillRemoteBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AAbilitySkillRemoteBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	Execute_SetHitAble(this, false);
	Execute_ClearHitTargets(this);
	MovementComponent->Velocity = FVector::ZeroVector;
}

void AAbilitySkillRemoteBase::Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerCharacter, InItem);

	Execute_SetHitAble(this, true);
	MovementComponent->Velocity = GetActorRotation().RotateVector(InitialVelocity);
}

bool AAbilitySkillRemoteBase::CanHitTarget_Implementation(AActor* InTarget)
{
	return Super::CanHitTarget_Implementation(InTarget);
}

void AAbilitySkillRemoteBase::OnHitTarget_Implementation(AActor* InTarget, const FHitResult& InHitResult)
{
	Super::OnHitTarget_Implementation(InTarget, InHitResult);

	UObjectPoolModuleBPLibrary::DespawnObject(this);
}

void AAbilitySkillRemoteBase::ClearHitTargets_Implementation()
{
	Super::ClearHitTargets_Implementation();
}

void AAbilitySkillRemoteBase::SetHitAble_Implementation(bool bValue)
{
	Super::SetHitAble_Implementation(bValue);

	SphereComponent->SetGenerateOverlapEvents(bValue);
}
