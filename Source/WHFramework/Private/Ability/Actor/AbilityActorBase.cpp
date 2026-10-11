// Fill out your copyright notice in the Description page of Project Settings.

#include "Ability/Actor/AbilityActorBase.h"

#include "Ability/AbilityModuleStatics.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Attributes/ActorAttributeSetBase.h"
#include "Ability/Components/AbilitySystemComponentBase.h"
#include "Ability/Inventory/AbilityInventoryBase.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"
#include "Asset/AssetModuleStatics.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Components/BoxComponent.h"

AAbilityActorBase::AAbilityActorBase(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

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

void AAbilityActorBase::OnSpawn_Implementation(const FParameter& InParam)
{
	if(const FAbilityActorSpawnParameter* Param = InParam.GetPtr<FAbilityActorSpawnParameter>())
	{
		AssetID = Param->AssetID;
	}

	Super::OnSpawn_Implementation(InParam);

	InitializeAbilities();
}

void AAbilityActorBase::OnDespawn_Implementation(EObjectDespawnMode InMode)
{
	BindInventory(nullptr);
	Super::OnDespawn_Implementation(InMode);

	AssetID = FPrimaryAssetId();
	Name = NAME_None;
	Level = 0;

	BirthTransform = FTransform::Identity;
	

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

void AAbilityActorBase::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	Super::LoadData(InSaveData, InPhase);

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
			UInventoryWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
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
	SaveData = Super::ToData().GetRef<FSceneActorSaveData>();

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


void AAbilityActorBase::BindInventory(UAbilityInventoryBase* InInventory)
{
	if (Inventory == InInventory)
	{
		return;
	}
	UInventoryWorldSubsystem* NextSubsystem = InInventory ? Cast<UInventoryWorldSubsystem>(InInventory->GetOuter()) : nullptr;
	UInventoryWorldSubsystem* WorldSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
	if (InInventory && (!NextSubsystem || NextSubsystem != WorldSubsystem || !NextSubsystem->AcquireLease(InInventory->GetInventoryId())))
	{
		UE_LOG(LogTemp, Error, TEXT("Inventory binding rejected for %s"), *GetName());
		return;
	}
	UAbilityInventoryBase* PreviousInventory = Inventory;
	Inventory = InInventory;
	if (PreviousInventory)
	{
		if (UInventoryWorldSubsystem* PreviousSubsystem = Cast<UInventoryWorldSubsystem>(PreviousInventory->GetOuter()))
		{
			PreviousSubsystem->ReleaseLease(PreviousInventory->GetInventoryId());
		}
	}
}
