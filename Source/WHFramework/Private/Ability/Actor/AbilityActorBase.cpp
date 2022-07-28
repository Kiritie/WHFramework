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
	
	InitializeAbilitySystem();

	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(Iter).AddUObject(this, &AAbilityActorBase::OnAttributeChange);
	}
}

void AAbilityActorBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);
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

FGameplayAbilitySpecHandle AAbilityActorBase::AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel /*= 1*/)
{
	if (AbilitySystem && InAbility)
	{
		FGameplayAbilitySpecDef SpecDef = FGameplayAbilitySpecDef();
		SpecDef.Ability = InAbility;
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(SpecDef, InLevel);
		return AbilitySystem->GiveAbility(AbilitySpec);
	}
	return FGameplayAbilitySpecHandle();
}

bool AAbilityActorBase::ActiveAbility(FGameplayAbilitySpecHandle SpecHandle, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbility(SpecHandle, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityActorBase::ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilityByClass(AbilityClass, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityActorBase::ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilitiesByTag(GameplayTagContainer, bAllowRemoteActivation);
	}
	return false;
}

void AAbilityActorBase::CancelAbility(UAbilityBase* Ability)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbility(Ability);
	}
}

void AAbilityActorBase::CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilityHandle(AbilityHandle);
	}
}

void AAbilityActorBase::CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilities(&WithTags, &WithoutTags, Ignore);
	}
}

void AAbilityActorBase::CancelAllAbilities(UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAllAbilities(Ignore);
	}
}

FActiveGameplayEffectHandle AAbilityActorBase::ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass)
{
	if (AbilitySystem)
	{
		auto effectContext = AbilitySystem->MakeEffectContext();
		effectContext.AddSourceObject(this);
		auto specHandle = AbilitySystem->MakeOutgoingSpec(EffectClass, GetLevelV(), effectContext);
		if (specHandle.IsValid())
		{
			return AbilitySystem->ApplyGameplayEffectSpecToSelf(*specHandle.Data.Get());
		}
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityActorBase::ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityActorBase::ApplyEffectBySpec(const FGameplayEffectSpec& Spec)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
	}
	return FActiveGameplayEffectHandle();
}

bool AAbilityActorBase::RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove/*=-1*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}
	return false;
}

void AAbilityActorBase::GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetActiveAbilitiesWithTags(AbilityTags, ActiveAbilities);
	}
}

bool AAbilityActorBase::GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo)
{
	return false;
}

FGameplayAbilitySpec AAbilityActorBase::GetAbilitySpecByHandle(FGameplayAbilitySpecHandle Handle)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec AAbilityActorBase::GetAbilitySpecByGEHandle(FActiveGameplayEffectHandle GEHandle)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromGEHandle(GEHandle))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec AAbilityActorBase::GetAbilitySpecByClass(TSubclassOf<UGameplayAbility> InAbilityClass)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromClass(InAbilityClass))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

FGameplayAbilitySpec AAbilityActorBase::GetAbilitySpecByInputID(int32 InputID)
{
	if (AbilitySystem)
	{
		if(FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromInputID(InputID))
		{
			return *Spec;
		}
	}
	return FGameplayAbilitySpec();
}

void AAbilityActorBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
}

FGameplayAttributeData AAbilityActorBase::GetAttributeData(FGameplayAttribute InAttribute)
{
	return AttributeSet->GetAttributeData(InAttribute);
}

float AAbilityActorBase::GetAttributeValue(FGameplayAttribute InAttribute)
{
	return AttributeSet->GetAttributeValue(InAttribute);
}

void AAbilityActorBase::SetAttributeValue(FGameplayAttribute InAttribute, float InValue)
{
	AttributeSet->SetAttributeValue(InAttribute, InValue);
}

TArray<FGameplayAttribute> AAbilityActorBase::GetAllAttributes() const
{
	return AttributeSet->GetAllAttributes();
}

TArray<FGameplayAttribute> AAbilityActorBase::GetPersistentAttributes() const
{
	return AttributeSet->GetPersistentAttributes();
}

UAbilitySystemComponent* AAbilityActorBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}
