#include "Ability/Actor/AbilityActorDataBase.h"


UAbilityActorDataBase::UAbilityActorDataBase()
{
	Type = FName("Actor");
	MaxCount = 1;
	Class = nullptr;
	PEClass = nullptr;
	Radius = 40.f;
	HalfHeight = 40.f;
	InventoryProfile = nullptr;
}

void UAbilityActorDataBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void UAbilityActorDataBase::OnReset_Implementation()
{
	Super::OnReset_Implementation();
}

