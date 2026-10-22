// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"
#include "Ability/Inventory/InventoryItemAssetCatalog.h"
#include "Ability/Inventory/InventoryWorldSubsystem.h"
#include "Engine/World.h"

#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Attributes/VitalityAttributeSetBase.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Ability/Item/Equip/AbilityEquipBase.h"
#include "Ability/Item/Prop/AbilityPropBase.h"
#include "Ability/Item/Raw/AbilityRawBase.h"
#include "Asset/AssetModuleStatics.h"
#include "ObjectPool/ObjectPoolModuleStatics.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "Scene/Container/SceneContainerInterface.h"
#include "Voxel/VoxelModule.h"

IMPLEMENTATION_MODULE(UAbilityModule)

// Sets default values
UAbilityModule::UAbilityModule()
{
	ModuleName = FName("AbilityModule");
	ModuleDisplayName = FText::FromString(TEXT("Ability Module"));
	
	AttributeColorMap.Add(GET_GAMEPLAYATTRIBUTE(UVitalityAttributeSetBase, PhysicsDamage), FLinearColor::Red);
	AttributeColorMap.Add(GET_GAMEPLAYATTRIBUTE(UVitalityAttributeSetBase, MagicDamage), FLinearColor(0.45f, 0.f, 1.f));
	AttributeColorMap.Add(GET_GAMEPLAYATTRIBUTE(UVitalityAttributeSetBase, FallDamage), FLinearColor::Red);
	AttributeColorMap.Add(GET_GAMEPLAYATTRIBUTE(UVitalityAttributeSetBase, RealDamage), FLinearColor::White);
}

UAbilityModule::~UAbilityModule()
{
	TERMINATION_MODULE(UAbilityModule)
}

#if WITH_EDITOR
void UAbilityModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAbilityModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UAbilityModule)
}
#endif

void UAbilityModule::OnInitialize()
{
	Super::OnInitialize();

	UAssetModuleStatics::AddStaticObject(FName("EAbilityItemType"), FStaticObject(UEnum::StaticClass(), TEXT("/Script/WHFramework.EAbilityItemType")));
}

void UAbilityModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAbilityModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UAbilityModule::OnPause()
{
	Super::OnPause();
}

void UAbilityModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAbilityModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
}

FLinearColor UAbilityModule::GetAttributeColor(const FGameplayAttribute& InAttribute) const
{
	if(AttributeColorMap.Contains(InAttribute))
	{
		return AttributeColorMap[InAttribute];
	}
	return FLinearColor();
}

void UAbilityModule::SetAttributeColor(const FGameplayAttribute& InAttribute, const FLinearColor& InColor)
{
	AttributeColorMap.Emplace(InAttribute, InColor);
}

AAbilityItemBase* UAbilityModule::SpawnAbilityItem(FAbilityItem InItem, FVector InLocation, FRotator InRotation, ISceneContainerInterface* InContainer)
{
	if(AAbilityItemBase* Item = SpawnAbilityItem(InItem))
	{
		Item->SetActorLocationAndRotation(InLocation, InRotation);
		if(InContainer)
		{
			InContainer->AddSceneActor(Item);
		}
		return Item;
	}
	return nullptr;
}

AAbilityItemBase* UAbilityModule::SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor)
{
	UWorld* World = InOwnerActor ? InOwnerActor->GetWorld() : GetWorld();
	const UInventoryWorldSubsystem* Subsystem = World ? World->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
	const UInventoryItemAssetCatalog* Catalog = Subsystem ? Subsystem->GetItemAssets() : nullptr;
	const UAbilityItemDataBase* Definition = Catalog ? Catalog->Find(InItem.ItemId) : nullptr;
	if (!Definition || InItem.Level < 0 || InItem.Level > Definition->MaxLevel)
	{
		return nullptr;
	}
	TSubclassOf<AAbilityItemBase> ProjectionClass;
	if (const UAbilityEquipDataBase* Equipment = Cast<UAbilityEquipDataBase>(Definition))
	{
		ProjectionClass = Equipment->EquipClass;
	}
	else if (const UAbilityPropDataBase* Prop = Cast<UAbilityPropDataBase>(Definition))
	{
		ProjectionClass = Prop->PropClass;
	}
	else if (const UAbilityRawDataBase* Raw = Cast<UAbilityRawDataBase>(Definition))
	{
		ProjectionClass = Raw->RawClass;
	}
	if (!ProjectionClass)
	{
		return nullptr;
	}
	FAbilityItemSpawnParameter Parameter(InOwnerActor, InItem.ItemId, InItem.Level);
	Parameter.WorldContext = World;
	return UObjectPoolModuleStatics::SpawnObject<AAbilityItemBase>(Parameter, ProjectionClass);
}


AActor* UAbilityModule::SpawnAbilityActor(const FParameter& InSaveData, ISceneContainerInterface* InContainer)
{
	const FActorSaveData& SaveData = InSaveData.GetRef<FActorSaveData>();
	const UInventoryWorldSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UInventoryWorldSubsystem>() : nullptr;
	const UInventoryItemAssetCatalog* Catalog = Subsystem ? Subsystem->GetItemAssets() : nullptr;
	const UAbilityActorDataBase* Definition = Catalog ? Catalog->FindAs<UAbilityActorDataBase>(SaveData.AssetID) : nullptr;
	if (!Definition || !Definition->Class)
	{
		return nullptr;
	}
	AActor* Actor = UObjectPoolModuleStatics::SpawnObject<AActor>(
		FAbilityActorSpawnParameter(this, SaveData.SpawnTransform, SaveData.ActorID, SaveData.AssetID), Definition->Class);
	ISaveDataAgentInterface* SaveAgent = Cast<ISaveDataAgentInterface>(Actor);
	if (!SaveAgent)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
		return nullptr;
	}
	SaveAgent->LoadSaveData(InSaveData);
	if (InContainer)
	{
		InContainer->AddSceneActor(Actor);
	}
	return Actor;
}
