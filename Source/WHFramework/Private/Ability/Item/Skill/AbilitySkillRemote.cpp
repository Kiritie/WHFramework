// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Skill/AbilitySkillRemote.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

// Sets default values
AAbilitySkillRemote::AAbilitySkillRemote()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	SphereComponent->SetupAttachment(RootComponent);
	SphereComponent->SetCollisionProfileName(TEXT("DW_Weapon"));
	SphereComponent->SetSphereRadius(50);
	SphereComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilitySkillRemote::OnBeginOverlap);
	SetRootComponent(SphereComponent);

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);

	MovementComponent = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComponent"));
	MovementComponent->bRotationFollowsVelocity = true;
	MovementComponent->ProjectileGravityScale = 0;
	MovementComponent->BounceVelocityStopSimulatingThreshold = false;

	DurationTime = 10;
	InitialVelocity = FVector(3000, 0, 0);
}

// Called when the game starts or when spawned
void AAbilitySkillRemote::BeginPlay()
{
	Super::BeginPlay();

}

void AAbilitySkillRemote::Initialize(AAbilityCharacterBase* InOwnerCharacter, const FPrimaryAssetId& InSkillID)
{
	Super::Initialize(InOwnerCharacter, InSkillID);

	MovementComponent->Velocity = GetActorRotation().RotateVector(InitialVelocity);
}

void AAbilitySkillRemote::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	OnHitTarget(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

// Called every frame
void AAbilitySkillRemote::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}
