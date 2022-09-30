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

void AAbilitySkillRemoteBase::Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerCharacter, InItem);

	if(Execute_IsVisible(this))
	{
		SphereComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MovementComponent->Velocity = GetActorRotation().RotateVector(InitialVelocity);
	}
}

void AAbilitySkillRemoteBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
}

void AAbilitySkillRemoteBase::OnDespawn_Implementation()
{
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MovementComponent->Velocity = FVector::ZeroVector;

	Super::OnDespawn_Implementation();
}

void AAbilitySkillRemoteBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor != GetOwnerCharacter())
	{
		OnHitTarget(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
		UObjectPoolModuleBPLibrary::DespawnObject(this);
	}
}

void AAbilitySkillRemoteBase::OnHitTarget_Implementation(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}
