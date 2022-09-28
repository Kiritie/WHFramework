// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/Item/Equip/AbilityEquipBase.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Global/GlobalBPLibrary.h"
#include "Components/BoxComponent.h"

AAbilityEquipBase::AAbilityEquipBase()
{
	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(false);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetRelativeLocationAndRotation(FVector(40, 0, 0), FRotator(0, 0, 0));
	BoxComponent->SetRelativeScale3D(FVector(1.5625f, 1, 0.316406f));
	BoxComponent->SetBoxExtent(FVector(24, 10, 32));
	BoxComponent->SetCollisionProfileName(TEXT("Weapon"));
	BoxComponent->SetGenerateOverlapEvents(false);
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityEquipBase::OnBeginOverlap);
}

void AAbilityEquipBase::Initialize_Implementation(AAbilityCharacterBase* InOwnerCharacter, const FAbilityItem& InItem)
{
	Super::Initialize_Implementation(InOwnerCharacter, InItem);
}

void AAbilityEquipBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(OtherActor != GetOwnerCharacter())
	{
		OnHitTarget(OverlappedComponent, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
	}
}

void AAbilityEquipBase::OnHitTarget_Implementation(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
}

void AAbilityEquipBase::SetCollisionEnable(bool InValue)
{
	BoxComponent->SetGenerateOverlapEvents(InValue);
}

void AAbilityEquipBase::OnAssemble_Implementation()
{
	
}

void AAbilityEquipBase::OnDischarge_Implementation()
{
	
}
