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
	SphereComponent = CreateDefaultSubobject<USphereComponent>(FName("SphereComponent"));
	USceneComponent* PreviousRoot = RootComponent;
	SetRootComponent(SphereComponent);
	if (PreviousRoot)
	{
		PreviousRoot->SetupAttachment(SphereComponent);
	}
	SphereComponent->SetCollisionProfileName(TEXT("Projectile"));
	SphereComponent->SetSphereRadius(50.f);
	SphereComponent->SetGenerateOverlapEvents(false);
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityProjectileRemoteBase::OnBeginOverlap);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(FName("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->SetUpdatedComponent(SphereComponent);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ThisClass::OnProjectileStopped);
	ProjectileMovement->ProjectileGravityScale = 0;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = 0.f;
	ProjectileMovement->Velocity = FVector::ZeroVector;

	MaxDurationTime = 10.f;
	InitialVelocity = 3000.f;
}

void AAbilityProjectileRemoteBase::OnSpawn_Implementation(const FParameter& InParam)
{
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	ProjectileMovement->SetUpdatedComponent(SphereComponent);
	Super::OnSpawn_Implementation(InParam);
}

void AAbilityProjectileRemoteBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();
	Super::OnDespawn_Implementation(InMode);
}

void AAbilityProjectileRemoteBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(CanHitTarget(OtherActor))
	{
		OnHitTarget(OtherActor, SweepResult);
	}
}

void AAbilityProjectileRemoteBase::Launch_Implementation(FVector InDirection)
{
	Super::Launch_Implementation(InDirection);
	if (HasAuthority() && IsLaunched() && IsHitAble() && FMath::IsFinite(InitialVelocity) && InitialVelocity > 0.f)
	{
		ProjectileMovement->SetUpdatedComponent(SphereComponent);
		ProjectileMovement->Velocity = GetActorForwardVector() * FMath::Clamp(InitialVelocity, 1.f, 100000.f);
		ProjectileMovement->Activate(true);
	}
}

bool AAbilityProjectileRemoteBase::CanHitTarget(AActor* InTarget) const
{
	return Super::CanHitTarget(InTarget);
}

void AAbilityProjectileRemoteBase::OnHitTarget(AActor* InTarget, const FHitResult& InHitResult)
{
	Super::OnHitTarget(InTarget, InHitResult);
}

bool AAbilityProjectileRemoteBase::IsHitAble() const
{
	return Super::IsHitAble() && SphereComponent->GetGenerateOverlapEvents();
}

void AAbilityProjectileRemoteBase::SetHitAble(bool bValue)
{
	Super::SetHitAble(bValue);
	const bool bEnable = bHitEnabled && HasAuthority();
	SphereComponent->SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(bEnable);
}


void AAbilityProjectileRemoteBase::OnProjectileStopped(const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}
	if (CanHitTarget(Hit.GetActor()))
	{
		OnHitTarget(Hit.GetActor(), Hit);
	}
	else
	{
		Destroy();
	}
}
