// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpSkeletalMesh.h"

#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"

AAbilityPickUpSkeletalMesh::AAbilityPickUpSkeletalMesh()
{
	BoxComponent->SetBoxExtent(FVector(20.f));

	MeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(FName("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpSkeletalMesh::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpSkeletalMesh::GetMeshComponent() const
{
	return MeshComponent;
}
