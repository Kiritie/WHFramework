// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Actor/AbilityActorBase.h"

#include "Ability/Abilities/AbilityBase.h"
#include "Ability/Attributes/AttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"

AAbilityActorBase::AAbilityActorBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	// AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	// AbilitySystem->SetIsReplicated(true);
	// AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet = CreateDefaultSubobject<UAttributeSetBase>(FName("AttributeSet"));

	ActorID = FGuid::NewGuid();
	Container = nullptr;
}

void AAbilityActorBase::BeginPlay()
{
	Super::BeginPlay();
}

void AAbilityActorBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);

	InitializeAbilitySystem();
}

void AAbilityActorBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();
}

void AAbilityActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AAbilityActorBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!AttributeSet) return;

	if(Ar.ArIsSaveGame && AttributeSet->GetPersistentAttributes().Num() > 0)
	{
		float BaseValue = 0.f;
		float CurrentValue = 0.f;
		for(FGameplayAttribute& Attribute : AttributeSet->GetPersistentAttributes())
		{
			if(FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(AttributeSet))
			{
				if(Ar.IsLoading())
				{
					Ar << BaseValue;
					Ar << CurrentValue;
					AttributeData->SetBaseValue(BaseValue);
					AttributeData->SetCurrentValue(CurrentValue);
				}
				else if(Ar.IsSaving())
				{
					BaseValue = AttributeData->GetBaseValue();
					CurrentValue = AttributeData->GetCurrentValue();
					Ar << BaseValue;
					Ar << CurrentValue;
				}
			}
		}
	}
}

void AAbilityActorBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
}

bool AAbilityActorBase::SetLevelV(int32 InLevel)
{
	if(Level != InLevel)
	{
		Level = InLevel;
		return true;
	}
	return false;
}

UAbilitySystemComponent* AAbilityActorBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
