// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpSkill.h"

#include "PaperSpriteComponent.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Components/StaticMeshComponent.h"

AAbilityPickUpSkill::AAbilityPickUpSkill()
{
	MeshComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAbilityPickUpSkill::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpSkill::GetMeshComponent() const
{
	return MeshComponent;
}
