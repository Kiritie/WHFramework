// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpStaticMesh.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpStaticMesh::AAbilityPickUpStaticMesh()
{
	BoxComponent->SetBoxExtent(FVector(20.f));

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(FName("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpStaticMesh::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpStaticMesh::GetMeshComponent() const
{
	return MeshComponent;
}
