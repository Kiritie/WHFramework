// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/Item/Equip/AbilityEquipBase.h"
#include "Ability/Item/Prop/AbilityPropBase.h"
#include "Ability/Item/Raw/AbilityRawBase.h"
#include "Ability/Projectile/AbilityProjectileBase.h"
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
	switch (InItem.GetType())
	{
		case EAbilityItemType::Prop:
		{
			return UObjectPoolModuleStatics::SpawnObject<AAbilityPropBase>(InOwnerActor, { &InItem }, false, InItem.GetData<UAbilityPropDataBase>().PropClass);
		}
		case EAbilityItemType::Equip:
		{
			return UObjectPoolModuleStatics::SpawnObject<AAbilityEquipBase>(InOwnerActor, { &InItem }, false, InItem.GetData<UAbilityEquipDataBase>().EquipClass);
		}
		case EAbilityItemType::Raw:
		{
			return UObjectPoolModuleStatics::SpawnObject<AAbilityRawBase>(InOwnerActor, { &InItem }, false, InItem.GetData<UAbilityRawDataBase>().RawClass);
		}
		default: break;
	}
	return nullptr;
}

AAbilityPickUpBase* UAbilityModule::SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(!InItem.IsValid()) return nullptr;

	static FPickUpSaveData SaveData;
	SaveData.Item = InItem;
	SaveData.Location = InLocation;

	return SpawnAbilityPickUp(&SaveData, InContainer);
}

AAbilityPickUpBase* UAbilityModule::SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	const auto& SaveData = InSaveData->CastRef<FPickUpSaveData>();

	const auto& ItemData = SaveData.Item.GetData<UAbilityItemDataBase>();
	
	if(AAbilityPickUpBase* PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpBase>(nullptr, nullptr, false, ItemData.PickUpClass))
	{
		PickUp->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(PickUp);
		}
		return PickUp;
	}
	return nullptr;
}

AAbilityProjectileBase* UAbilityModule::SpawnAbilityProjectile(const TSubclassOf<AAbilityProjectileBase>& InClass, AActor* InOwnerActor, const FGameplayAbilitySpecHandle& InAbilityHandle)
{
	return UObjectPoolModuleStatics::SpawnObject<AAbilityProjectileBase>(InOwnerActor, { &InAbilityHandle }, false, InClass);
}

AActor* UAbilityModule::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FActorSaveData>();
	if(AActor* Actor = UObjectPoolModuleStatics::SpawnObject<AActor>(nullptr, { SaveData.ActorID, SaveData.AssetID }, false, SaveData.GetData<UAbilityActorDataBase>().Class))
	{
		Cast<ISaveDataAgentInterface>(Actor)->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Actor);
		}
		return Actor;
	}
	return nullptr;
}
