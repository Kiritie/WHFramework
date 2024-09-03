// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/AbilityPawnBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Death.h"
#include "Ability/Pawn/States/AbilityPawnState_Default.h"
#include "Components/BoxComponent.h"
#include "FSM/Components/FSMComponent.h"
#include "Common/CommonStatics.h"
#include "Scene/SceneModuleStatics.h"
#include "Voxel/VoxelModule.h"
#include "Ability/AbilityModuleStatics.h"
#include "Ability/Pawn/AbilityPawnInventoryBase.h"

AAbilityPawnBase::AAbilityPawnBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;

	AutoPossessAI = EAutoPossessAI::Disabled;

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UVitalityAttributeSetBase>(FName("AttributeSet"));
		
	Inventory = CreateDefaultSubobject<UAbilityPawnInventoryBase>(FName("Inventory"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);
	Interaction->SetRelativeLocation(FVector(0, 0, 0));

	FSM = CreateDefaultSubobject<UFSMComponent>(FName("FSM"));
	FSM->GroupName = FName("Vitality");
	FSM->DefaultState = UAbilityPawnState_Default::StaticClass();
	FSM->States.Add(UAbilityPawnState_Default::StaticClass());
	FSM->States.Add(UAbilityPawnState_Death::StaticClass());

	// stats
	RaceID = NAME_None;
	Level = 0;
}

void AAbilityPawnBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityPawnBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		RefreshAttributes();
	}
}

void AAbilityPawnBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityPawnBase::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

void AAbilityPawnBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	Super::OnSpawn_Implementation(InOwner, InParams);

	InitializeAbilities();

	SpawnDefaultController();

	FSM->SwitchDefaultState();
}

void AAbilityPawnBase::OnDespawn_Implementation(bool bRecovery)
{
	RaceID = NAME_None;
	Level = 0;

	Inventory->UnloadSaveData();
}

void AAbilityPawnBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
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
		SetLevelA(SaveData.Level);

		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);

		if(!SaveData.IsSaved())
		{
			ResetData();
		}
	}
}

FSaveData* AAbilityPawnBase::ToData()
{
	static FVitalitySaveData SaveData;
	SaveData = FVitalitySaveData();

	SaveData.ActorID = ActorID;
	SaveData.AssetID = AssetID;
	SaveData.Name = Name;
	SaveData.RaceID = RaceID;
	SaveData.Level = Level;

	SaveData.InventoryData = Inventory->GetSaveDataRef<FInventorySaveData>(true);

	SaveData.SpawnLocation = GetActorLocation();
	SaveData.SpawnRotation = GetActorRotation();

	return &SaveData;
}

void AAbilityPawnBase::ResetData()
{
	SetHealth(GetMaxHealth());
}

void AAbilityPawnBase::OnFiniteStateRefresh(UFiniteStateBase* InCurrentState)
{
}

void AAbilityPawnBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

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

void AAbilityPawnBase::Death(IAbilityVitalityInterface* InKiller)
{
	if(InKiller)
	{
		InKiller->Kill(this);
	}
	FSM->SwitchStateByClass<UAbilityPawnState_Death>({ InKiller });
}

void AAbilityPawnBase::Kill(IAbilityVitalityInterface* InTarget)
{
	
}

void AAbilityPawnBase::Revive(IAbilityVitalityInterface* InRescuer)
{
	FSM->SwitchDefaultState();
}

bool AAbilityPawnBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return false;
}

void AAbilityPawnBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityPawnBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityPawnBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassivity)
{
	
}

void AAbilityPawnBase::OnAdditionItem(const FAbilityItem& InItem)
{
	
}

void AAbilityPawnBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AAbilityPawnBase::OnCancelItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AAbilityPawnBase::OnAssembleItem(const FAbilityItem& InItem)
{

}

void AAbilityPawnBase::OnDischargeItem(const FAbilityItem& InItem)
{

}

void AAbilityPawnBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector tmpPos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) tmpPos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, tmpPos, Container.GetInterface());
}

void AAbilityPawnBase::OnSelectItem(const FAbilityItem& InItem)
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

void AAbilityPawnBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

bool AAbilityPawnBase::IsDead(bool bCheckDying) const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::StateTag_Vitality_Dead) || bCheckDying && IsDying();
}

bool AAbilityPawnBase::IsDying() const
{
	return AbilitySystem->HasMatchingGameplayTag(GameplayTags::StateTag_Vitality_Dying);
}

bool AAbilityPawnBase::SetLevelA(int32 InLevel)
{
	const auto& VitalityData = GetPawnData<UAbilityPawnDataBase>();
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

FString AAbilityPawnBase::GetHeadInfo() const
{
	return FString::Printf(TEXT("Lv.%d \"%s\" "), Level, *Name.ToString());
}

float AAbilityPawnBase::GetRadius() const
{
	return FMath::Max(BoxComponent->GetScaledBoxExtent().X, BoxComponent->GetScaledBoxExtent().Y);
}

float AAbilityPawnBase::GetHalfHeight() const
{
	return BoxComponent->GetScaledBoxExtent().Z;
}

UAttributeSetBase* AAbilityPawnBase::GetAttributeSet() const
{
	return AttributeSet;
}

UAbilitySystemComponent* AAbilityPawnBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

UInteractionComponent* AAbilityPawnBase::GetInteractionComponent() const
{
	return Interaction;
}

UAbilityInventoryBase* AAbilityPawnBase::GetInventory() const
{
	return Inventory;
}

bool AAbilityPawnBase::IsPlayer() const
{
	return UCommonStatics::GetPlayerPawn() == this;
}

bool AAbilityPawnBase::IsEnemy(IAbilityPawnInterface* InTarget) const
{
	return !InTarget->GetRaceID().IsEqual(RaceID);
}

bool AAbilityPawnBase::IsTargetAble_Implementation(APawn* InPlayerPawn) const
{
	return !IsDead();
}

void AAbilityPawnBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if(InAttributeChangeData.Attribute == GetExpAttribute())
	{
		if(InAttributeChangeData.NewValue >= GetMaxExp())
		{
			SetLevelA(GetLevelA() + 1);
			SetExp(0.f);
		}
	}
}

void AAbilityPawnBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-LocalDamageDone);

	if (GetHealth() <= 0.f)
	{
		Death(Cast<IAbilityVitalityInterface>(SourceActor));
	}

	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalDamageDone), FColor::White, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
}

void AAbilityPawnBase::HandleRecovery(const float LocalRecoveryDone, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(LocalRecoveryDone);
	
	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalRecoveryDone), FColor::Green, EWorldTextStyle::Normal, GetActorLocation(), FVector(20.f));
}

void AAbilityPawnBase::HandleInterrupt(const float InterruptDuration, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
}
