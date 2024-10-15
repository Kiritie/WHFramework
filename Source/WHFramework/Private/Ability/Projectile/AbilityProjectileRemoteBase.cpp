// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Projectile/AbilityProjectileRemoteBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"

// Sets default values
AAbilityProjectileRemoteBase::AAbilityProjectileRemoteBase()
{
	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	SphereComponent->SetupAttachment(RootComponent);
	SphereComponent->SetCollisionProfileName(TEXT("Projectile"));
	SphereComponent->SetSphereRadius(50.f);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityProjectileRemoteBase::OnBeginOverlap);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = false;
	ProjectileMovement->Velocity = FVector::ZeroVector;

	MaxDurationTime = 10.f;
	InitialVelocity = 3000.f;
}

void AAbilityProjectileRemoteBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(CanHitTarget(OtherActor))
	{
		OnHitTarget(OtherActor, SweepResult);
	}
}

void AAbilityProjectileRemoteBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);
}

void AAbilityProjectileRemoteBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	ProjectileMovement->Velocity = FVector::ZeroVector;
}

void AAbilityProjectileRemoteBase::Initialize_Implementation(AActor* InOwnerActor, const FGameplayAbilitySpecHandle& InAbilityHandle)
{
	Super::Initialize_Implementation(InOwnerActor, InAbilityHandle);
}

void AAbilityProjectileRemoteBase::Launch_Implementation(FVector InDirection)
{
	Super::Launch_Implementation(InDirection);

	ProjectileMovement->Velocity = GetActorRotation().RotateVector(FVector(InitialVelocity, 0.f, 0.f));
}

bool AAbilityProjectileRemoteBase::CanHitTarget(AActor* InTarget) const
{
	return Super::CanHitTarget(InTarget);
}

void AAbilityProjectileRemoteBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	Super::OnHitTarget(InTarget, InHitResult);

	UObjectPoolModuleStatics::DespawnObject(this);
}

void AAbilityProjectileRemoteBase::ClearHitTargets()
{
	Super::ClearHitTargets();
}

void AAbilityProjectileRemoteBase::SetHitAble(bool bValue)
{
	Super::SetHitAble(bValue);

	SphereComponent->SetGenerateOverlapEvents(bValue);
}
