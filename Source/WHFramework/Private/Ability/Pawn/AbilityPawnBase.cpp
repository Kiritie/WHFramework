// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Pawn/AbilityPawnBase.h"

#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Ability/Pawn/States/AbilityPawnState_Death.h"
#include "Ability/Pawn/States/AbilityPawnState_Default.h"
#include "Asset/AssetModuleStatics.h"
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

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(BoxComponent);

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

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;

	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	RaceID = NAME_None;
	Level = 0;
	GenerateVoxelID = FPrimaryAssetId();
}

void AAbilityPawnBase::SetActorVisible_Implementation(bool bInVisible)
{
	bVisible = bInVisible;
	GetRootComponent()->SetVisibility(bInVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for(auto Iter : AttachedActors)
	{
		if(Iter && Iter->Implements<USceneActorInterface>())
		{
			ISceneActorInterface::Execute_SetActorVisible(Iter, bInVisible);
		}
	}
}

bool AAbilityPawnBase::OnGenerateVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnGenerateVoxel(InVoxelHitResult);
}

bool AAbilityPawnBase::OnDestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	return IVoxelAgentInterface::OnDestroyVoxel(InVoxelHitResult);
}

void AAbilityPawnBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(0))
	{
		ActorID = InParams[0].GetPointerValueRef<FGuid>();
	}
	if(InParams.IsValidIndex(1))
	{
		AssetID = InParams[1].GetPointerValueRef<FPrimaryAssetId>();
	}

	USceneModuleStatics::AddSceneActor(this);

	InitializeAbilitySystem();

	FSM->SwitchDefaultState();
}

void AAbilityPawnBase::OnDespawn_Implementation(bool bRecovery)
{
	FSM->SwitchState(nullptr);

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
		SetLevelV(SaveData.Level);

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

void AAbilityPawnBase::OnFiniteStateChanged(UFiniteStateBase* InFiniteState)
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
		FSM->GetStateByClass<UAbilityPawnState_Death>()->Killer = InKiller;
		InKiller->Kill(this);
	}
	FSM->SwitchStateByClass<UAbilityPawnState_Death>();
}

void AAbilityPawnBase::Kill(IAbilityVitalityInterface* InTarget)
{
	if(InTarget == this)
	{
		Death(this);
	}
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

bool AAbilityPawnBase::SetLevelV(int32 InLevel)
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

UAbilityPawnDataBase& AAbilityPawnBase::GetPawnData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityPawnDataBase>(AssetID);
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

bool AAbilityPawnBase::IsTargetable_Implementation() const
{
	return !IsDead();
}

void AAbilityPawnBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
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
			USceneModuleStatics::SpawnWorldText(FString::FromInt(DeltaValue), FColor::Green, DeltaValue < GetMaxHealth() ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));
		}
	}
}

void AAbilityPawnBase::HandleDamage(EDamageType DamageType, const float LocalDamageDone, bool bHasCrited, bool bHasDefend, FHitResult HitResult, const FGameplayTagContainer& SourceTags, AActor* SourceActor)
{
	ModifyHealth(-LocalDamageDone);

	USceneModuleStatics::SpawnWorldText(FString::FromInt(LocalDamageDone), FColor::White, !bHasCrited ? EWorldTextStyle::Normal : EWorldTextStyle::Stress, GetActorLocation(), FVector(20.f));

	if (GetHealth() <= 0.f)
	{
		Death(Cast<IAbilityVitalityInterface>(SourceActor));
	}
}
