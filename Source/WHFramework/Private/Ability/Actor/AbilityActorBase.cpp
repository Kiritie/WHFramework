// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Actor/AbilityActorBase.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Attributes/ActorAttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Asset/AssetModuleStatics.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/BoxComponent.h"

AAbilityActorBase::AAbilityActorBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(BoxComponent);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UActorAttributeSetBase>(FName("AttributeSet"));
		
	Inventory = CreateDefaultSubobject<UAbilityInventoryBase>(FName("Inventory"));

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);

	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	Level = 0;
}

void AAbilityActorBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(1))
	{
		AssetID = InParams[1].GetPointerValueRef<FPrimaryAssetId>();
	}

	Super::OnSpawn_Implementation(InOwner, InParams);

	InitializeAbilities();
}

void AAbilityActorBase::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	Level = 0;
	Inventory->UnloadSaveData();
}

void AAbilityActorBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityActorBase::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		RefreshAttributes();
	}
}

void AAbilityActorBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityActorBase::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
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

void AAbilityActorBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	auto& SaveData = InSaveData->CastRef<FActorSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData.SpawnTransform);
	}
	if(PHASEC(InPhase, EPhase::All))
	{
		SetNameA(SaveData.Name);
		SetLevelA(SaveData.Level);

		Inventory->LoadSaveData(&SaveData.InventoryData, InPhase);
	}
}

FSaveData* AAbilityActorBase::ToData()
{
	static FActorSaveData SaveData;
	SaveData = Super::ToData()->CastRef<FSceneActorSaveData>();

	SaveData.AssetID = AssetID;
	SaveData.Name = Name;
	SaveData.Level = Level;

	SaveData.InventoryData = Inventory->GetSaveDataRef<FInventorySaveData>(true);

	SaveData.SpawnTransform = GetActorTransform();

	return &SaveData;
}

void AAbilityActorBase::ResetData()
{
}

bool AAbilityActorBase::CanInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent)
{
	return false;
}

void AAbilityActorBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityActorBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityActorBase::OnInteract(EInteractAction InInteractAction, IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	
}

void AAbilityActorBase::OnAdditionItem(const FAbilityItem& InItem)
{
}

void AAbilityActorBase::OnRemoveItem(const FAbilityItem& InItem)
{
}

void AAbilityActorBase::OnActiveItem(const FAbilityItem& InItem, bool bPassive, bool bSuccess)
{

}

void AAbilityActorBase::OnDeactiveItem(const FAbilityItem& InItem, bool bPassive)
{

}

void AAbilityActorBase::OnDiscardItem(const FAbilityItem& InItem, bool bInPlace)
{
	FVector tmpPos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) tmpPos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, tmpPos, Container.GetInterface());
}

void AAbilityActorBase::OnSelectItem(ESlotSplitType InSplitType, const FAbilityItem& InItem)
{
	
}

void AAbilityActorBase::OnAuxiliaryItem(const FAbilityItem& InItem)
{

}

void AAbilityActorBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
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

UAbilityActorDataBase& AAbilityActorBase::GetActorData() const
{
	return UAssetModuleStatics::LoadPrimaryAssetRef<UAbilityActorDataBase>(AssetID);
}

UAttributeSetBase* AAbilityActorBase::GetAttributeSet() const
{
	return AttributeSet;
}

UShapeComponent* AAbilityActorBase::GetCollisionComponent() const
{
	return BoxComponent;
}

UAbilitySystemComponent* AAbilityActorBase::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

bool AAbilityActorBase::SetLevelA(int32 InLevel)
{
	const auto& ActorData = GetActorData<UAbilityActorDataBase>();
	InLevel = ActorData.ClampLevel(InLevel);

	if(Level != InLevel)
	{
		Level = InLevel;

		auto EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		auto SpecHandle = AbilitySystem->MakeOutgoingSpec(ActorData.PEClass, InLevel, EffectContext);
		if (SpecHandle.IsValid())
		{
			AbilitySystem->BP_ApplyGameplayEffectSpecToSelf(SpecHandle);
		}
		ResetData();
		return true;
	}
	return false;
}

float AAbilityActorBase::GetRadius() const
{
	return FMath::Max(BoxComponent->GetScaledBoxExtent().X, BoxComponent->GetScaledBoxExtent().Y);
}

float AAbilityActorBase::GetHalfHeight() const
{
	return BoxComponent->GetScaledBoxExtent().Z;
}
