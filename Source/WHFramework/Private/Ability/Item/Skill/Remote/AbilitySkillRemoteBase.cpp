// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/Remote/AbilitySkillRemoteBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
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
	if(CanHitTarget(OtherActor))
	{
		OnHitTarget(OtherActor, SweepResult);
	}
}

void AAbilitySkillRemoteBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void AAbilitySkillRemoteBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	SetHitAble(false);
	ClearHitTargets();
	MovementComponent->Velocity = FVector::ZeroVector;
}

void AAbilitySkillRemoteBase::Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerActor, InItem);

	SetHitAble(true);
	MovementComponent->Velocity = GetActorRotation().RotateVector(InitialVelocity);
}

bool AAbilitySkillRemoteBase::CanHitTarget(AActor* InTarget) const
{
	return Super::CanHitTarget(InTarget);
}

void AAbilitySkillRemoteBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	Super::OnHitTarget(InTarget, InHitResult);

	UObjectPoolModuleStatics::DespawnObject(this);
}

void AAbilitySkillRemoteBase::ClearHitTargets()
{
	Super::ClearHitTargets();
}

void AAbilitySkillRemoteBase::SetHitAble(bool bValue)
{
	Super::SetHitAble(bValue);

	SphereComponent->SetGenerateOverlapEvents(bValue);
}
