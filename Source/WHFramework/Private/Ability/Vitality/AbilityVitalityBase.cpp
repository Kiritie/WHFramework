// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Components/VitalityInteractionComponent.h"
#include "Ability/Abilities/VitalityAbilityBase.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Default.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Global/GlobalBPLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
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
	FSM->GroupName = FName("Vitality");
	FSM->States.Add(UAbilityVitalityState_Default::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());
	FSM->DefaultState = UAbilityVitalityState_Default::StaticClass();
	
	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;
	EXP = 50;
	BaseEXP = 100;
	EXPFactor = 2.f;

	OwnerChunk = nullptr;
}

// Called when the game starts or when spawned
void AAbilityVitalityBase::BeginPlay()
{
	Super::BeginPlay();
	
	AbilitySystem->InitAbilityActorInfo(this, this);
	for(auto Iter : AttributeSet->GetAllAttributes())
	{
		AbilitySystem->GetGameplayAttributeValueChangeDelegate(Iter).AddUObject(this, &AAbilityVitalityBase::OnAttributeChange);
	}
}

void AAbilityVitalityBase::OnFiniteStateChanged(UFiniteStateBase* InFiniteState)
{
}

void AAbilityVitalityBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void AAbilityVitalityBase::OnDespawn_Implementation()
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

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData)
{

}

FSaveData* AAbilityVitalityBase::ToData()
{
	return nullptr;
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

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DeadTag) || bCheckDying && IsDying();
}

bool AAbilityVitalityBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DyingTag);
}

void AAbilityVitalityBase::SetNameV(FName InName)
{
	Name = InName;
}

void AAbilityVitalityBase::SetRaceID(FName InRaceID)
{
	RaceID = InRaceID;
}

void AAbilityVitalityBase::SetLevelV(int32 InLevel)
{
	Level = InLevel;
}

void AAbilityVitalityBase::SetEXP(int32 InEXP)
{
	EXP = InEXP;
}

int32 AAbilityVitalityBase::GetMaxEXP() const
{
	int32 MaxEXP = BaseEXP;
	for (int i = 0; i < Level - 1; i++)
	{
		MaxEXP *= EXPFactor;
	}
	return MaxEXP;
}

int32 AAbilityVitalityBase::GetTotalEXP() const
{
	int32 TotalEXP = BaseEXP;
	for (int i = 0; i < Level - 1; i++)
	{
		TotalEXP += TotalEXP * EXPFactor;
	}
	return EXP + TotalEXP - GetMaxEXP();
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

float AAbilityVitalityBase::GetHealth() const
{
	return AttributeSet->GetHealth();
}

void AAbilityVitalityBase::SetHealth(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetHealthAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityVitalityBase::GetMaxHealth() const
{
	return AttributeSet->GetMaxHealth();
}

void AAbilityVitalityBase::SetMaxHealth(float InValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetMaxHealthAttribute(), EGameplayModOp::Override, InValue);
}

float AAbilityVitalityBase::GetPhysicsDamage() const
{
	return AttributeSet->GetPhysicsDamage();
}

float AAbilityVitalityBase::GetMagicDamage() const
{
	return AttributeSet->GetMagicDamage();
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
}

UAbilitySystemComponent* AAbilityVitalityBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
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
			if (ModifierInfo.Attribute == AttributeSet->GetHealthAttribute())
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

void AAbilityVitalityBase::ModifyHealth(float InDeltaValue)
{
	AbilitySystem->ApplyModToAttributeUnsafe(AttributeSet->GetHealthAttribute(), EGameplayModOp::Additive, InDeltaValue);
}

void AAbilityVitalityBase::ModifyEXP(float InDeltaValue)
{
	const int32 MaxEXP = GetMaxEXP();
	int32 TempEXP = GetEXP();
	TempEXP += InDeltaValue;
	if (InDeltaValue > 0.f)
	{
		if (TempEXP >= MaxEXP)
		{
			SetLevelV(Level + 1);
			TempEXP -= MaxEXP;
		}
	}
	else
	{
		if (TempEXP < 0.f)
		{
			TempEXP = 0.f;
		}
	}
	SetEXP(TempEXP);
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
		if (!AMainModule::GetModuleByClass<AVoxelModule>()->VoxelTraceSingle(InVoxelItem, chunk->IndexToLocation(index), HitResult))
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
	const FIndex index = chunk->LocationToIndex(InVoxelHitResult.Point - AVoxelModule::GetWorldData()->GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);
	const FVoxelItem& voxelItem = chunk->GetVoxelItem(index);

	if(!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && voxelItem != InVoxelHitResult.VoxelItem)
	{
		FHitResult HitResult;
		if (!AMainModule::GetModuleByClass<AVoxelModule>()->VoxelTraceSingle(InVoxelItem, chunk->IndexToLocation(index), HitResult))
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

UInteractionComponent* AAbilityVitalityBase::GetInteractionComponent() const
{
	return Interaction;
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == AttributeSet->GetHealthAttribute())
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
