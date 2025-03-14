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

	BirthTransform = FTransform::Identity;
}

void AAbilityActorBase::OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams)
{
	if(InParams.IsValidIndex(1))
	{
		AssetID = InParams[1];
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

	BirthTransform = FTransform::Identity;
	
	Inventory->UnloadSaveData();
}

void AAbilityActorBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityActorBase::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	RefreshAttributes();
}

void AAbilityActorBase::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityActorBase::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}

void AAbilityActorBase::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(!AttributeSet) return;

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << Level;
		}
		else if(Ar.IsSaving())
		{
			Ar << Level;
		}
		AttributeSet->SerializeAttributes(Ar);
	}
}

void AAbilityActorBase::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

	auto& SaveData = InSaveData->CastRef<FActorSaveData>();

	if(PHASEC(InPhase, EPhase::Primary))
	{
		SetActorTransform(SaveData.SpawnTransform);
		if(!SaveData.IsSaved())
		{
			BirthTransform = SaveData.SpawnTransform;
		}
		else
		{
			BirthTransform = SaveData.BirthTransform;
		}
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
	SaveData.BirthTransform = BirthTransform;

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
	if(InItem == PAID_EXP)
	{
		ModifyExp(InItem.Count);
	}
}

void AAbilityActorBase::OnRemoveItem(const FAbilityItem& InItem)
{
}

void AAbilityActorBase::OnPreChangeItem(const FAbilityItem& InOldItem)
{
	
}

void AAbilityActorBase::OnChangeItem(const FAbilityItem& InNewItem)
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
	FVector Pos = GetActorLocation() + FMath::RandPointInBox(FBox(FVector(-20.f, -20.f, -10.f), FVector(20.f, 20.f, 10.f)));
	if(!bInPlace) Pos += GetActorForwardVector() * (GetRadius() + 35.f);
	UAbilityModuleStatics::SpawnAbilityPickUp(InItem, Pos, Container.GetInterface());
}

void AAbilityActorBase::OnSelectItem(const FAbilityItem& InItem)
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
			const float Exp = InAttributeChangeData.NewValue - GetMaxExp();
			SetLevelA(GetLevelA() + 1);
			SetExp(Exp);
		}
	}
}

void AAbilityActorBase::OnActorAttached(AActor* InActor)
{
	
}

void AAbilityActorBase::OnActorDetached(AActor* InActor)
{
	
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

UMeshComponent* AAbilityActorBase::GetMeshComponent() const
{
	return nullptr;
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
		Inventory->ResetItems();
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

float AAbilityActorBase::GetDistance(AActor* InTargetActor, bool bIgnoreRadius /*= true*/, bool bIgnoreZAxis /*= true*/) const
{
	if(!InTargetActor) return -1;

	IAbilityActorInterface* TargetAbilityActor = Cast<IAbilityActorInterface>(InTargetActor);

	return FVector::Distance(FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z), FVector(InTargetActor->GetActorLocation().X, InTargetActor->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetActor->GetActorLocation().Z)) - (bIgnoreRadius ? 0 : TargetAbilityActor->GetRadius());
}
