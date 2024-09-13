// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpSprite.h"

#include "Ability/PickUp/Widget/WidgetAbilityPickUpSprite.h"
#include "Components/BoxComponent.h"
#include "Widget/World/WorldWidgetComponent.h"

AAbilityPickUpSprite::AAbilityPickUpSprite()
{
	BoxComponent->SetBoxExtent(FVector(12.f, 3.f, 12.f));

	MeshComponent = CreateDefaultSubobject<UWorldWidgetComponent>(TEXT("MeshComponent"));
	MeshComponent->SetupAttachment(RootComponent);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		
	static ConstructorHelpers::FClassFinder<UWidgetAbilityPickUpSprite> SpriteWidgetClassFinder(TEXT("/Script/UMGEditor.WidgetBlueprint'/WHFramework/Ability/Blueprints/Widget/WBP_PickUpSprite_Base.WBP_PickUpSprite_Base_C'"));
	if(SpriteWidgetClassFinder.Succeeded())
	{
		MeshComponent->SetWorldWidgetClass(SpriteWidgetClassFinder.Class);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpriteMatFinder(TEXT("/Script/Engine.MaterialInstanceConstant'/WHFramework/Common/Materials/MI_DefaultWidget_Lit_Masked.MI_DefaultWidget_Lit_Masked'"));
	if(SpriteMatFinder.Succeeded())
	{
		MeshComponent->SetMaterial(0, SpriteMatFinder.Object);
	}
}

void AAbilityPickUpSprite::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	if(UWidgetAbilityPickUpSprite* PickUpSprite = Cast<UWidgetAbilityPickUpSprite>(MeshComponent->GetWorldWidget()))
	{
		PickUpSprite->InitAbilityItem(Item);
	}
}

void AAbilityPickUpSprite::OnPickUp(IAbilityPickerInterface* InPicker)
{
	Super::OnPickUp(InPicker);
}

UMeshComponent* AAbilityPickUpSprite::GetMeshComponent() const
{
	return MeshComponent;
}
