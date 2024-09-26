// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/PickUp/AbilityPickUpBase.h"

#include "Ability/AbilityModule.h"
#include "Ability/PickUp/AbilityPickerInterface.h"
#include "Common/Components/FallingMovementComponent.h"
#include "Common/Components/FollowingMovementComponent.h"
#include "Common/Interaction/InteractionComponent.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Components/BoxComponent.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "Scene/SceneModuleStatics.h"

AAbilityPickUpBase::AAbilityPickUpBase()
{
	PrimaryActorTick.bCanEverTick = true;

	BoxComponent = CreateDefaultSubobject<UInteractionComponent>(FName("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionProfileName(TEXT("PickUp"));
	BoxComponent->SetBoxExtent(FVector(15.f));
	BoxComponent->OnComponentBeginOverlap.AddDynamic(this, &AAbilityPickUpBase::OnBeginOverlap);

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetBoxExtent(FVector(50.f));
	Interaction->SetInteractable(false);
	Interaction->AddInteractAction(EInteractAction::PickUp);

	RotatingMovement = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("RotatingMovement"));
	RotatingMovement->RotationRate = FRotator(0.f, 180.f, 0.f);

	FallingMovement = CreateDefaultSubobject<UFallingMovementComponent>(TEXT("FallingMovement"));
	
	FollowingMovement = CreateDefaultSubobject<UFollowingMovementComponent>(TEXT("FollowingMovement"));

	Item = FAbilityItem::Empty;
}

void AAbilityPickUpBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	FallingMovement->SetTraceChannel(USceneModuleStatics::GetTraceMapping(FName("PickUp")).GetTraceChannel());
}

void AAbilityPickUpBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	Interaction->SetInteractable(true);
}

void AAbilityPickUpBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Item = FAbilityItem::Empty;

	FollowingMovement->SetFollowingTarget(nullptr);
	Interaction->SetInteractable(false);
}

void AAbilityPickUpBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	const auto& SaveData = InSaveData->CastRef<FPickUpSaveData>();

	Item = SaveData.Item;
	SetActorLocationAndRotation(SaveData.Location, FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f));
}

FSaveData* AAbilityPickUpBase::ToData()
{
	static FPickUpSaveData SaveData;
	SaveData = FPickUpSaveData();

	SaveData.Item = Item;
	SaveData.Location = GetActorLocation();

	return &SaveData;
}

void AAbilityPickUpBase::OnPickUp(IAbilityPickerInterface* InPicker)
{
	if(InPicker && InPicker->OnPickUp(this))
	{
		UObjectPoolModuleStatics::DespawnObject(this);
	}
}

bool AAbilityPickUpBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	switch (InInteractAction)
	{
		case EInteractAction::PickUp:
		{
			if(IAbilityPickerInterface* Picker = Cast<IAbilityPickerInterface>(InInteractionAgent))
			{
				return Item.IsValid() && !Picker->IsAutoPickUp();
			}
		}
		default: break;
	}
	return false;
}

void AAbilityPickUpBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
	if(!Item.IsValid()) return;

	if(IAbilityPickerInterface* Picker = Cast<IAbilityPickerInterface>(InInteractionAgent))
	{
		if(Picker->IsAutoPickUp())
		{
			FallingMovement->SetActive(false);
			FollowingMovement->SetFollowingTarget(Cast<AActor>(Picker));
		}
	}
}

void AAbilityPickUpBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityPickUpBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity)
{
	if(bPassivity)
	{
		switch (InInteractAction)
		{
			case EInteractAction::PickUp:
			{
				if(IAbilityPickerInterface* Picker = Cast<IAbilityPickerInterface>(InInteractionAgent))
				{
					OnPickUp(Picker);
				}
				break;
			}
			default: break;
		}
	}
}

void AAbilityPickUpBase::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(!Item.IsValid()) return;

	if(IAbilityPickerInterface* Picker = Cast<IAbilityPickerInterface>(OtherActor))
	{
		if(Picker->IsAutoPickUp() && !OtherComp->IsA<UInteractionComponent>())
		{
			OnPickUp(Picker);
		}
	}
}

FBox AAbilityPickUpBase::GetComponentsBoundingBox(bool bNonColliding, bool bIncludeFromChildActors) const
{
	return BoxComponent->Bounds.GetBox();
}

UInteractionComponent* AAbilityPickUpBase::GetInteractionComponent() const
{
	return Interaction;
}
