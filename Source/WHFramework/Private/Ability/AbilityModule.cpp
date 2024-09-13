// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/Item/Equip/AbilityEquipBase.h"
#include "Ability/Item/Prop/AbilityPropBase.h"
#include "Ability/Item/Raw/AbilityRawBase.h"
#include "Ability/Item/Skill/AbilitySkillBase.h"
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
	AAbilityItemBase* Item = nullptr;
	switch (InItem.GetType())
	{
		case EAbilityItemType::Prop:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilityPropBase>(nullptr, nullptr, false, InItem.GetData<UAbilityPropDataBase>().PropClass);
			break;
		}
		case EAbilityItemType::Equip:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilityEquipBase>(nullptr, nullptr, false, InItem.GetData<UAbilityEquipDataBase>().EquipClass);
			break;
		}
		case EAbilityItemType::Skill:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilitySkillBase>(nullptr, nullptr, false, InItem.GetData<UAbilitySkillDataBase>().SkillClass);
			break;
		}
		case EAbilityItemType::Raw:
		{
			Item = UObjectPoolModuleStatics::SpawnObject<AAbilityRawBase>(nullptr, nullptr, false, InItem.GetData<UAbilityRawDataBase>().RawClass);
			break;
		}
		default: break;
	}

	if(Item)
	{
		Item->SetActorLocationAndRotation(InLocation, InRotation);
		if(InContainer)
		{
			InContainer->AddSceneActor(Item);
		}
	}
	return Item;
}

AAbilityItemBase* UAbilityModule::SpawnAbilityItem(FAbilityItem InItem, AActor* InOwnerActor)
{
	AAbilityItemBase* Item = SpawnAbilityItem(InItem);
	if(Item)
	{
		Item->Initialize(InOwnerActor, InItem);
	}
	return Item;
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
	
	AAbilityPickUpBase* PickUp = UObjectPoolModuleStatics::SpawnObject<AAbilityPickUpBase>(nullptr, nullptr, false, ItemData.PickUpClass);

	if(PickUp)
	{
		PickUp->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(PickUp);
		}
	}
	return PickUp;
}

AActor* UAbilityModule::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FActorSaveData>();
	if(AActor* Actor = UObjectPoolModuleStatics::SpawnObject<AActor>(nullptr, { &SaveData.ActorID, &SaveData.AssetID }, false, SaveData.GetItemData<UAbilityActorDataBase>().Class))
	{
		Cast<ISaveDataInterface>(Actor)->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Actor);
		}
		return Actor;
	}
	return nullptr;
}
