﻿// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Components/VitalityInteractionComponent.h"
#include "Ability/Abilities/VitalityAbilityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Default.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

// Sets default values
AAbilityVitalityBase::AAbilityVitalityBase()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	SetRootComponent(BoxComponent);

	// AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	// AbilitySystem->SetIsReplicated(true);
	// AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));
	
	Interaction = CreateDefaultSubobject<UVitalityInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->bAutoSwitchDefault = true;
	FSM->GroupName = FName("Vitality");
	FSM->DefaultState = UAbilityVitalityState_Default::StaticClass();
	FSM->States.Add(UAbilityVitalityState_Default::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());
	
	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;
}

void AAbilityVitalityBase::BeginPlay()
{
	Super::BeginPlay();

	InitializeAbilitySystem();

	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(Iter).AddUObject(this, &AAbilityVitalityBase::OnAttributeChange);
	}
}

void AAbilityVitalityBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);

	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::OnDespawn_Implementation()
{
	Super::OnDespawn_Implementation();

	FSM->SwitchState(nullptr);

	RaceID = NAME_None;
	Level = 0;
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	auto SaveData = InSaveData->CastRef<FVitalitySaveData>();
	
	if(bForceMode)
	{
		AssetID = SaveData.ID;
		SetRaceID(SaveData.RaceID);
		SetLevelV(SaveData.Level);
		SetActorLocation(SaveData.SpawnLocation);
		SetActorRotation(SaveData.SpawnRotation);
	}

	SetNameV(SaveData.Name);
}

FSaveData* AAbilityVitalityBase::ToData()
{
	static FVitalitySaveData SaveData;
	SaveData.Reset();

	SaveData.ID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();

	return &SaveData;
}

void AAbilityVitalityBase::OnFiniteStateChanged(UFiniteStateBase* InFiniteState)
{
}

// Called every frame
void AAbilityVitalityBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if(IsDead()) return;
}

void AAbilityVitalityBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!GetAttributeSet<UVitalityAttributeSetBase>()) return;

	if(Ar.ArIsSaveGame && GetAttributeSet<UVitalityAttributeSetBase>()->GetPersistentAttributes().Num() > 0)
	{
		float BaseValue = 0.f;
		float CurrentValue = 0.f;
		for(FGameplayAttribute& Attribute : GetAttributeSet<UVitalityAttributeSetBase>()->GetPersistentAttributes())
		{
			if(FGameplayAttributeData* AttributeData = Attribute.GetGameplayAttributeData(GetAttributeSet<UVitalityAttributeSetBase>()))
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

void AAbilityVitalityBase::Death(IAbilityVitalityInterface* InKiller)
{
	FSM->GetStateByClass<UAbilityVitalityState_Death>()->Killer = InKiller;
	FSM->SwitchStateByClass<UAbilityVitalityState_Death>();
}

void AAbilityVitalityBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityVitalityBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

bool AAbilityVitalityBase::CanInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
	return false;
}

void AAbilityVitalityBase::OnInteract(IInteractionAgentInterface* InInteractionAgent, EInteractAction InInteractAction)
{
}

FGameplayAbilitySpecHandle AAbilityVitalityBase::AcquireAbility(TSubclassOf<UAbilityBase> InAbility, int32 InLevel /*= 1*/)
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

bool AAbilityVitalityBase::ActiveAbility(FGameplayAbilitySpecHandle SpecHandle, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbility(SpecHandle, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityVitalityBase::ActiveAbilityByClass(TSubclassOf<UAbilityBase> AbilityClass, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilityByClass(AbilityClass, bAllowRemoteActivation);
	}
	return false;
}

bool AAbilityVitalityBase::ActiveAbilityByTag(const FGameplayTagContainer& GameplayTagContainer, bool bAllowRemoteActivation /*= false*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->TryActivateAbilitiesByTag(GameplayTagContainer, bAllowRemoteActivation);
	}
	return false;
}

void AAbilityVitalityBase::CancelAbility(UAbilityBase* Ability)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbility(Ability);
	}
}

void AAbilityVitalityBase::CancelAbilityByHandle(const FGameplayAbilitySpecHandle& AbilityHandle)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilityHandle(AbilityHandle);
	}
}

void AAbilityVitalityBase::CancelAbilities(const FGameplayTagContainer& WithTags, const FGameplayTagContainer& WithoutTags, UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAbilities(&WithTags, &WithoutTags, Ignore);
	}
}

void AAbilityVitalityBase::CancelAllAbilities(UAbilityBase* Ignore/*=nullptr*/)
{
	if (AbilitySystem)
	{
		AbilitySystem->CancelAllAbilities(Ignore);
	}
}

FActiveGameplayEffectHandle AAbilityVitalityBase::ApplyEffectByClass(TSubclassOf<UGameplayEffect> EffectClass)
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

FActiveGameplayEffectHandle AAbilityVitalityBase::ApplyEffectBySpecHandle(const FGameplayEffectSpecHandle& SpecHandle)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
	return FActiveGameplayEffectHandle();
}

FActiveGameplayEffectHandle AAbilityVitalityBase::ApplyEffectBySpec(const FGameplayEffectSpec& Spec)
{
	if (AbilitySystem)
	{
		return AbilitySystem->ApplyGameplayEffectSpecToSelf(Spec);
	}
	return FActiveGameplayEffectHandle();
}

bool AAbilityVitalityBase::RemoveEffect(FActiveGameplayEffectHandle Handle, int32 StacksToRemove/*=-1*/)
{
	if (AbilitySystem)
	{
		return AbilitySystem->RemoveActiveGameplayEffect(Handle, StacksToRemove);
	}
	return false;
}

void AAbilityVitalityBase::GetActiveAbilities(FGameplayTagContainer AbilityTags, TArray<UAbilityBase*>& ActiveAbilities)
{
	if (AbilitySystem)
	{
		AbilitySystem->GetActiveAbilitiesWithTags(AbilityTags, ActiveAbilities);
	}
}

bool AAbilityVitalityBase::GetAbilityInfo(TSubclassOf<UAbilityBase> AbilityClass, FAbilityInfo& OutAbilityInfo)
{
	if (AbilitySystem && AbilityClass != nullptr)
	{
		float Cost = 0;
		float Cooldown = 0;
		EAbilityCostType CostType = EAbilityCostType::None;
		UAbilityBase* Ability = AbilityClass.GetDefaultObject();
		if (Ability->GetCostGameplayEffect()->Modifiers.Num() > 0)
		{
			const FGameplayModifierInfo ModifierInfo = Ability->GetCostGameplayEffect()->Modifiers[0];
			ModifierInfo.ModifierMagnitude.GetStaticMagnitudeIfPossible(1, Cost);
			if (ModifierInfo.Attribute == GetAttributeSet<UVitalityAttributeSetBase>()->GetHealthAttribute())
			{
				CostType = EAbilityCostType::Health;
			}
		}
		Ability->GetCooldownGameplayEffect()->DurationMagnitude.GetStaticMagnitudeIfPossible(1, Cooldown);
		OutAbilityInfo = FAbilityInfo(CostType, Cost, Cooldown);
		return true;
	}
	return false;
}

FGameplayAbilitySpec AAbilityVitalityBase::GetAbilitySpecByHandle(FGameplayAbilitySpecHandle Handle)
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

FGameplayAbilitySpec AAbilityVitalityBase::GetAbilitySpecByGEHandle(FActiveGameplayEffectHandle GEHandle)
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

FGameplayAbilitySpec AAbilityVitalityBase::GetAbilitySpecByClass(TSubclassOf<UGameplayAbility> InAbilityClass)
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

FGameplayAbilitySpec AAbilityVitalityBase::GetAbilitySpecByInputID(int32 InputID)
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

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DeadTag) || bCheckDying && IsDying();
}

bool AAbilityVitalityBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DyingTag);
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

bool AAbilityVitalityBase::GenerateVoxel(FVoxelItem& InVoxelItem)
{
	bool bSuccess = false;
	AVoxelChunk* chunk = InVoxelItem.Owner;
	const FIndex index = InVoxelItem.Index;
	const FVoxelItem& voxelItem = chunk->GetVoxelItem(index);

	if(!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && voxelItem != InVoxelItem)
	{
		FHitResult HitResult;
		if (!UVoxelModuleBPLibrary::VoxelTraceSingle(InVoxelItem, chunk->IndexToLocation(index), HitResult))
		{
			if(voxelItem.IsValid())
			{
				bSuccess = chunk->ReplaceVoxel(voxelItem, InVoxelItem);
			}
			else
			{
				bSuccess = chunk->GenerateVoxel(index, InVoxelItem);
			}
		}
	}
	return bSuccess;
}

bool AAbilityVitalityBase::GenerateVoxel(FVoxelItem& InVoxelItem, const FVoxelHitResult& InVoxelHitResult)
{
	bool bSuccess = false;
	AVoxelChunk* chunk = InVoxelHitResult.GetOwner();
	const FIndex index = chunk->LocationToIndex(InVoxelHitResult.Point - UVoxelModuleBPLibrary::GetWorldData().GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);
	const FVoxelItem& voxelItem = chunk->GetVoxelItem(index);

	if(!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && voxelItem != InVoxelHitResult.VoxelItem)
	{
		FHitResult HitResult;
		if (!UVoxelModuleBPLibrary::VoxelTraceSingle(InVoxelItem, chunk->IndexToLocation(index), HitResult))
		{
			if(voxelItem.IsValid())
			{
				bSuccess = chunk->ReplaceVoxel(voxelItem, InVoxelItem);
			}
			else
			{
				bSuccess = chunk->GenerateVoxel(index, InVoxelItem);
			}
		}
	}
	return bSuccess;
}

bool AAbilityVitalityBase::DestroyVoxel(FVoxelItem& InVoxelItem)
{
	if (InVoxelItem.GetData<UVoxelData>().VoxelType != EVoxelType::Bedrock)
	{
		return InVoxelItem.Owner->DestroyVoxel(InVoxelItem);
	}
	return false;
}

bool AAbilityVitalityBase::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	AVoxelChunk* Chunk = InVoxelHitResult.GetOwner();
	const FVoxelItem& VoxelItem = InVoxelHitResult.VoxelItem;

	if (VoxelItem.GetData<UVoxelData>().VoxelType != EVoxelType::Bedrock)
	{
		return Chunk->DestroyVoxel(VoxelItem);
	}
	return false;
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
}

FGameplayAttributeData AAbilityVitalityBase::GetAttributeData(FGameplayAttribute InAttribute)
{
	return AttributeSet->GetAttributeData(InAttribute);
}

float AAbilityVitalityBase::GetAttributeValue(FGameplayAttribute InAttribute)
{
	return AttributeSet->GetAttributeValue(InAttribute);
}

void AAbilityVitalityBase::SetAttributeValue(FGameplayAttribute InAttribute, float InValue)
{
	AttributeSet->SetAttributeValue(InAttribute, InValue);
}

TArray<FGameplayAttribute> AAbilityVitalityBase::GetAllAttributes() const
{
	return AttributeSet->GetAllAttributes();
}

TArray<FGameplayAttribute> AAbilityVitalityBase::GetPersistentAttributes() const
{
	return AttributeSet->GetPersistentAttributes();
}

UAttributeSetBase* AAbilityVitalityBase::GetAttributeSet() const
{
	return AttributeSet;
}

UAbilitySystemComponent* AAbilityVitalityBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UInteractionComponent* AAbilityVitalityBase::GetInteractionComponent() const
{
	return Interaction;
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == GetAttributeSet<UVitalityAttributeSetBase>()->GetHealthAttribute())
	{
		const float DeltaValue = InAttributeChangeData.NewValue - InAttributeChangeData.OldValue;
		if(DeltaValue != 0.f)
		{
			USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(FMath::Abs(DeltaValue)), FColor::Green, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation());
		}
	}
}

void AAbilityVitalityBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	if (GetHealth() <= 0.f)
	{
		Death(Cast<IAbilityVitalityInterface>(SourceActor));
	}
}
