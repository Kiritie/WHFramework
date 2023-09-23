// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Vitality/AbilityVitalityBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Ability/Vitality/States/AbilityVitalityState_Death.h"
#include "Ability/Vitality/States/AbilityVitalityState_Default.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonBPLibrary.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/Datas/VoxelData.h"
#include "Ability/AbilityModuleBPLibrary.h"
#include "Ability/Vitality/AbilityVitalityInventoryBase.h"

AAbilityVitalityBase::AAbilityVitalityBase()
{
	PrimaryActorTick.bCanEverTick = true;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(BoxComponent);

	// AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	// AbilitySystem->SetIsReplicated(true);
	// AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));
			
	//Inventory = CreateDefaultSubobject<UVitalityInventory>(FName("Inventory"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Vitality");
	FSM->DefaultState = UAbilityVitalityState_Default::StaticClass();
	FSM->States.Add(UAbilityVitalityState_Default::StaticClass());
	FSM->States.Add(UAbilityVitalityState_Death::StaticClass());

	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;
	GenerateVoxelID = FPrimaryAssetId();
}

void AAbilityVitalityBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InParams);

	if(InParams.IsValidIndex(0))
	{
		AssetID = InParams[0].GetPointerValueRef<FPrimaryAssetId>();
	}

	InitializeAbilitySystem();

	FSM->SwitchDefaultState();
}

void AAbilityVitalityBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	FSM->SwitchState(nullptr);

	RaceID = NAME_None;
	Level = 0;

	Inventory->UnloadSaveData();
}

void AAbilityVitalityBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FVitalitySaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorLocation(SaveData.SpawnLocation);
		SetActorRotation(SaveData.SpawnRotation);
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		SetNameV(SaveData.Name);
		SetRaceID(SaveData.RaceID);
		SetLevelV(SaveData.Level);

		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);

		if(!SaveData.IsSaved())
		{
			ResetData();
		}
	}
}

FSaveData* AAbilityVitalityBase::ToData(bool bRefresh)
{
	static FVitalitySaveData SaveData;
	SaveData = FVitalitySaveData();

	SaveData.ID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.InventoryData = Inventory->GetSaveDataRef<FInventorySaveData>(true);

	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();

	return &SaveData;
}

void AAbilityVitalityBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityVitalityBase::OnFiniteStateChanged(UFiniteStateBase* InFiniteState)
{
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
	if(Ar.IsLoading())
	{
		RefreshAttributes();
	}
}

void AAbilityVitalityBase::Death(IAbilityVitalityInterface* InKiller)
{
	if(InKiller)
	{
		FSM->GetStateByClass<UAbilityVitalityState_Death>()->Killer = InKiller;
		InKiller->Kill(this);
	}
	FSM->SwitchStateByClass<UAbilityVitalityState_Death>();
}

void AAbilityVitalityBase::Kill(IAbilityVitalityInterface* InTarget)
{
	if(InTarget == this)
	{
		Death(this);
	}
}

void AAbilityVitalityBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState();
}

bool AAbilityVitalityBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return false;
}

void AAbilityVitalityBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityVitalityBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityVitalityBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity)
{
	
}

void AAbilityVitalityBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AAbilityVitalityBase::OnCancelItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AAbilityVitalityBase::OnAssembleItem(const FAbilityItem& InItem)
{

}

void AAbilityVitalityBase::OnDischargeItem(const FAbilityItem& InItem)
{

}

void AAbilityVitalityBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector tmpPos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) tmpPos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleBPLibrary::SpawnPickUp(InItem, tmpPos, Container.GetInterface());
}

void AAbilityVitalityBase::OnSelectItem(const FAbilityItem& InItem)
{
	if(InItem.IsValid() && InItem.GetType() == EAbilityItemType::Voxel)
	{
		SetGenerateVoxelID(InItem.ID);
	}
	else
	{
		SetGenerateVoxelID(FPrimaryAssetId());
	}
}

void AAbilityVitalityBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

bool AAbilityVitalityBase::OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnGenerateVoxel(InVoxelHitResult);
}

bool AAbilityVitalityBase::OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InVoxelHitResult);
}

bool AAbilityVitalityBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DeadTag) || bCheckDying && IsDying();
}

bool AAbilityVitalityBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GetVitalityData().DyingTag);
}

bool AAbilityVitalityBase::SetLevelV(int32 InLevel)
{
	const auto& VitalityData = GetVitalityData<UAbilityVitalityDataBase>();
	InLevel = FMath::Clamp(InLevel, 0, VitalityData.MaxLevel != -1 ? VitalityData.MaxLevel : InLevel);

	if(Level != InLevel)
	{
		Level = InLevel;

		auto EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		auto SpecHandle = AbilitySystem->MakeOutgoingSpec(VitalityData.PEClass, InLevel, EffectContext);
		if (SpecHandle.IsValid())
		{
			AbilitySystem->BP_ApplyGameplayEffectSpecToSelf(SpecHandle);
		}

		return true;
	}
	return false;
}

FString AAbilityVitalityBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

float AAbilityVitalityBase::GetRadius() const
{
	return FMath::Max(BoxComponent->GetScaledBoxExtent().X, BoxComponent->GetScaledBoxExtent().Y);
}

float AAbilityVitalityBase::GetHalfHeight() const
{
	return BoxComponent->GetScaledBoxExtent().Z;
}

UAbilityVitalityDataBase& AAbilityVitalityBase::GetVitalityData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UAbilityVitalityDataBase>(AssetID);
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

UAbilityInventoryBase* AAbilityVitalityBase::GetInventory() const
{
	return Inventory;
}

void AAbilityVitalityBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == AttributeSet->GetExpAttribute())
	{
		if(InAttributeChangeData.NewValue >= AttributeSet->GetMaxExp())
		{
			SetLevelV(GetLevelV() + 1);
			SetExp(0.f);
		}
	}
	else if(InAttributeChangeData.Attribute == AttributeSet->GetHealthAttribute())
	{
		const float DeltaValue = InAttributeChangeData.NewValue - InAttributeChangeData.OldValue;
		if(DeltaValue > 0.f)
		{
			USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(DeltaValue), FColor::Green, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
		}
	}
}

void AAbilityVitalityBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-LocalDamageDone);

	USceneModuleBPLibrary::SpawnWorldText(FString::FromInt(LocalDamageDone), FColor::White, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));

	if (GetHealth() <= 0.f)
	{
		Death(Cast<IAbilityVitalityInterface>(SourceActor));
	}
}
