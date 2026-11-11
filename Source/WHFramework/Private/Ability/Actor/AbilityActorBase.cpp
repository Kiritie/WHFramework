// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Actor/AbilityActorBase.h"
#include "WHFrameworkCoreMacros.h"
#include "Ability/AbilityActorTypes.h"
#include "Ability/AbilityActorSaveTypes.h"
#include "Common/LifecycleTypes.h"
#include "Scene/SceneActorSaveTypes.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Attributes/ActorAttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryModule.h"
#include "Engine/World.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Ability/Effects/EffectBase.h"
#include "Net/UnrealNetwork.h"

AAbilityActorBase::AAbilityActorBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(FName("BoxComponent"));
	BoxComponent->SetCollisionProfileName(FName("Vitality"));
	BoxComponent->SetBoxExtent(FVector(20, 20, 20));
	BoxComponent->CanCharacterStepUpOn = ECB_No;
	SetRootComponent(BoxComponent);

	AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponentBase>(FName("AbilitySystem"));
	AbilitySystem->SetIsReplicated(true);
	AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	AttributeSet = CreateDefaultSubobject<UActorAttributeSetBase>(FName("AttributeSet"));

	Inventory = nullptr;

	Interaction = CreateDefaultSubobject<UInteractionComponent>(FName("Interaction"));
	Interaction->SetupAttachment(RootComponent);

	// stats
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;
}

void AAbilityActorBase::OnRep_ActorID()
{
	if (AbilitySystem) AbilitySystem->SetAbilityEntityId(ActorID);
	Super::OnRep_ActorID();
}

void AAbilityActorBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if (HasAuthority() && AttributeSet)
	{
		const UAttributeSetBase* Defaults = AttributeSet->GetClass()->GetDefaultObject<UAttributeSetBase>();
		TArray<FGameplayAttribute> Attributes = AttributeSet->GetAllAttributes();
		Attributes.StableSort([](const FGameplayAttribute& A, const FGameplayAttribute& B) { return A.GetName().StartsWith(TEXT("Max")) && !B.GetName().StartsWith(TEXT("Max")); });
		for (const FGameplayAttribute& Attribute : Attributes)
		{
			if (const FGameplayAttributeData* Value = Attribute.GetGameplayAttributeData(Defaults))
			{
				AbilitySystem->SetNumericAttributeBase(Attribute, Value->GetBaseValue());
			}
		}
	}

	if(const FAbilityActorSpawnParameter* Param = InParam.GetPtr<FAbilityActorSpawnParameter>())
	{
		SetAssetID_Implementation(Param->AssetID);
	}
	else if (AssetID.IsValid()) AssetBinding.Bind(this, AssetID);

	Super::OnSpawn_Implementation(InParam);
	if (HasActorBegunPlay() && !Execute_IsInitialized(this)) return;

	AbilitySystem->SetAbilityEntityId(ActorID);
	InitializeAbilities();
}

void AAbilityActorBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	UninitializeAbilities();
	if (HasAuthority())
	{
		AbilitySystem->RemoveGrantSource(GetActorStatSourceId());
		AbilitySystem->ClearAllAbilities();
		for (FActiveGameplayEffectHandle Effect : AbilitySystem->GetActiveEffects(FGameplayEffectQuery()))
		{
			AbilitySystem->RemoveActiveGameplayEffect(Effect);
		}
	}
	AbilitySystem->SetAbilityEntityId(FGuid());
	BindInventory(nullptr);
	Super::OnDespawn_Implementation(InMode);

	AssetBinding.Reset();
	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;


}

void AAbilityActorBase::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
	if (!Execute_IsInitialized(this)) return;
	if (AssetID.IsValid()) AssetBinding.Bind(this, AssetID);
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
	BindInventory(nullptr);
	UninitializeAbilities();
	if (HasAuthority())
	{
		AbilitySystem->RemoveGrantSource(GetActorStatSourceId());
	}
	Super::OnTermination_Implementation();
	AssetBinding.Reset();
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

void AAbilityActorBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!HasAuthority() || !InSaveData.GetPtr<FActorSaveData>())
	{
		MarkLoadDataFailed();
		return;
	}
	Super::LoadData(InSaveData, InPhase);
	if (!WasLoadDataApplied()) return;

	auto& SaveData = InSaveData.GetRef<FActorSaveData>();

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

		if (HasAuthority() && SaveData.InventorySnapshot.InventoryId.IsValid())
		{
			UInventoryModule* Subsystem = GetWorld() ? UInventoryModule::GetPtr(GetWorld()) : nullptr;
			UAbilityInventoryBase* Resolved = Subsystem ? Subsystem->Resolve(SaveData.InventorySnapshot.InventoryId) : nullptr;
			if (!Resolved)
			{
				UE_LOG(LogTemp, Error, TEXT("Inventory restore requires the recovered world store: %s"), *SaveData.InventorySnapshot.InventoryId.ToString());
				return;
			}
			BindInventory(Resolved);
		}
	}
}

FParameter AAbilityActorBase::ToData()
{
	FActorSaveData SaveData;
	const FParameter ParentData = Super::ToData();
	const FSceneActorSaveData* Parent = ParentData.GetPtr<FSceneActorSaveData>();
	if (!Parent)
	{
		return FParameter();
	}
	SaveData = *Parent;

	SaveData.AssetID = AssetID;
	SaveData.Name = Name;
	SaveData.Level = Level;

	if (Inventory)
	{
		SaveData.InventorySnapshot = Inventory->MakeSnapshot();
	}

	SaveData.SpawnTransform = GetActorTransform();
	SaveData.BirthTransform = BirthTransform;

	return FParameter(MoveTemp(SaveData));
}

void AAbilityActorBase::ResetData()
{
}

void AAbilityActorBase::OnEnterInteract(IInteractionAgentInterface* InInteractionAgent)
{
}

void AAbilityActorBase::OnLeaveInteract(IInteractionAgentInterface* InInteractionAgent)
{
}


void AAbilityActorBase::OnAttributeChange(const FOnAttributeChangeData& InAttributeChangeData)
{
	if (!HasAuthority())
	{
		return;
	}

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

const UAbilityActorDataBase* AAbilityActorBase::GetActorData() const
{
	return Cast<UAbilityActorDataBase>(AssetBinding.Get());
}

void AAbilityActorBase::SetAssetID_Implementation(const FPrimaryAssetId& InID)
{
	AssetID = InID;
	AssetBinding.Bind(this, AssetID);
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
	if (!HasAuthority() || !AssetID.IsValid())
	{
		return false;
	}
	const UAbilityActorDataBase* Data = GetActorData();
	if (!Data) return false;
	InLevel = Data->ClampLevel(InLevel);
	if (InLevel == Level)
	{
		return false;
	}
	TArray<TSubclassOf<UEffectBase>> Effects;
	if (Data->PEClass)
	{
		Effects.Add(Data->PEClass);
	}
	if (!AbilitySystem->SetGrantSource(GetActorStatSourceId(), {}, Effects, InLevel))
	{
		return false;
	}
	Level = InLevel;
	ForceNetUpdate();
	return true;
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

	return FVector::Distance(FVector(GetActorLocation().X, GetActorLocation().Y, bIgnoreZAxis ? 0 : GetActorLocation().Z), FVector(InTargetActor->GetActorLocation().X, InTargetActor->GetActorLocation().Y, bIgnoreZAxis ? 0 : InTargetActor->GetActorLocation().Z)) - (bIgnoreRadius || !TargetAbilityActor ? 0.f : TargetAbilityActor->GetRadius());
}


FGuid AAbilityActorBase::GetActorStatSourceId() const
{
	return FGuid(ActorID.A ^ 0x649D7912, ActorID.B ^ 0xCC3545FE, ActorID.C ^ 0x88D5FE1A, ActorID.D ^ 0x9052C3F0);
}

void AAbilityActorBase::OnRep_ActorMetadata()
{
	AssetBinding.Bind(this, AssetID);
	RefreshAttributes();
}

void AAbilityActorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAbilityActorBase, AssetID);
	DOREPLIFETIME(AAbilityActorBase, Name);
	DOREPLIFETIME(AAbilityActorBase, Level);
}

void AAbilityActorBase::BindInventory(UAbilityInventoryBase* InInventory)
{
	if (Inventory == InInventory)
	{
		return;
	}
	UInventoryModule* NextSubsystem = InInventory ? Cast<UInventoryModule>(InInventory->GetOuter()) : nullptr;
	UInventoryModule* WorldSubsystem = GetWorld() ? UInventoryModule::GetPtr(GetWorld()) : nullptr;
	if (InInventory && (!NextSubsystem || NextSubsystem != WorldSubsystem || !NextSubsystem->AcquireLease(InInventory->GetInventoryId())))
	{
		UE_LOG(LogTemp, Error, TEXT("Inventory binding rejected for %s"), *GetName());
		return;
	}
	UAbilityInventoryBase* PreviousInventory = Inventory;
	Inventory = InInventory;
	if (PreviousInventory)
	{
		if (UInventoryModule* PreviousSubsystem = Cast<UInventoryModule>(PreviousInventory->GetOuter()))
		{
			PreviousSubsystem->ReleaseLease(PreviousInventory->GetInventoryId());
		}
	}
}
