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
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	SphereComponent->SetupAttachment(RootComponent);
	SphereComponent->SetCollisionProfileName(TEXT("Projectile"));
	SphereComponent->SetSphereRadius(50);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityProjectileRemoteBase::OnBeginOverlap);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->ProjectileGravityScale = 0;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = false;
	ProjectileMovement->Velocity = FVector::ZeroVector;

	DurationTime = 10;
	InitialVelocity = FVector(3000, 0, 0);
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

	ProjectileMovement->Velocity = GetActorRotation().RotateVector(InitialVelocity);
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
