// Fill out your copyright notice in the Description page of Project Settings.


#include "Ability/AbilityModule.h"

#include "Ability/Actor/AbilityActorDataBase.h"
#include "Ability/Character/AbilityCharacterBase.h"
#include "Ability/Character/AbilityCharacterDataBase.h"
#include "Ability/Item/Equip/AbilityEquipDataBase.h"
#include "Ability/Item/Prop/AbilityPropDataBase.h"
#include "Ability/Item/Raw/AbilityRawDataBase.h"
#include "Ability/Item/Skill/AbilitySkillDataBase.h"
#include "Ability/Pawn/AbilityPawnBase.h"
#include "Ability/Pawn/AbilityPawnDataBase.h"
#include "Ability/PickUp/AbilityPickUpBase.h"
#include "Ability/PickUp/AbilityPickUpEquip.h"
#include "Ability/PickUp/AbilityPickUpProp.h"
#include "Ability/PickUp/AbilityPickUpRaw.h"
#include "Ability/PickUp/AbilityPickUpSkill.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
#include "Ability/Vitality/AbilityVitalityBase.h"
#include "Ability/Vitality/AbilityVitalityDataBase.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Common/CommonBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "SaveGame/SaveGameModuleBPLibrary.h"
#include "Scene/Container/SceneContainerInterface.h"
		
IMPLEMENTATION_MODULE(AAbilityModule)

// Sets default values
AAbilityModule::AAbilityModule()
{
	ModuleName = FName("AbilityModule");
}

AAbilityModule::~AAbilityModule()
{
	TERMINATION_MODULE(AAbilityModule)
}

#if WITH_EDITOR
void AAbilityModule::OnGenerate()
{
	Super::OnGenerate();
}

void AAbilityModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AAbilityModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAbilityModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		DON_WITHINDEX(UCommonBPLibrary::GetEnumItemNum(TEXT("/Script/WHFramework.EAbilityItemType")), i,
			UAssetModuleBPLibrary::RegisterPrimaryAssetType(*UCommonBPLibrary::GetEnumValueAuthoredName(TEXT("/Script/WHFramework.EAbilityItemType"), i));
		);
	}
}

void AAbilityModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAbilityModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAbilityModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAbilityModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);
}

ECollisionChannel AAbilityModule::GetPickUpTraceChannel() const
{
	return ECC_MAX;
}

AAbilityPickUpBase* AAbilityModule::SpawnAbilityPickUp(FAbilityItem InItem, FVector InLocation, ISceneContainerInterface* InContainer)
{
	if(!InItem.IsValid()) return nullptr;

	FPickUpSaveData SaveData;
	SaveData.Item = InItem;
	SaveData.Location = InLocation;

	return SpawnAbilityPickUp(&SaveData, InContainer);
}

AAbilityPickUpBase* AAbilityModule::SpawnAbilityPickUp(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	const auto& SaveData = InSaveData->CastRef<FPickUpSaveData>();
	
	AAbilityPickUpBase* PickUp = nullptr;
	switch (SaveData.Item.GetType())
	{
		case EAbilityItemType::Voxel:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpVoxel>();
			break;
		}
		case EAbilityItemType::Prop:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpProp>(nullptr, SaveData.Item.GetData<UAbilityPropDataBase>().PropPickUpClass);
			break;
		}
		case EAbilityItemType::Equip:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpEquip>(nullptr, SaveData.Item.GetData<UAbilityEquipDataBase>().EquipPickUpClass);
			break;
		}
		case EAbilityItemType::Skill:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpSkill>(nullptr, SaveData.Item.GetData<UAbilitySkillDataBase>().SkillPickUpClass);
			break;
		}
		case EAbilityItemType::Raw:
		{
			PickUp = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPickUpRaw>(nullptr, SaveData.Item.GetData<UAbilityRawDataBase>().RawPickUpClass);
			break;
		}
		default: break;
	}

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

AAbilityActorBase* AAbilityModule::SpawnAbilityActor(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FActorSaveData>();
	if(AAbilityActorBase* Actor = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityActorBase>({ &SaveData.ActorID, &SaveData.AssetID }, SaveData.GetItemData<UAbilityActorDataBase>().Class))
	{
		Actor->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Actor);
		}
		return Actor;
	}
	return nullptr;
}

AAbilityVitalityBase* AAbilityModule::SpawnAbilityVitality(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FVitalitySaveData>();
	if(AAbilityVitalityBase* Vitality = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityVitalityBase>({ &SaveData.ActorID, &SaveData.AssetID }, SaveData.GetItemData<UAbilityVitalityDataBase>().Class))
	{
		Vitality->LoadSaveData(InSaveData);
		if(InContainer)
		{
			InContainer->AddSceneActor(Vitality);
		}
		return Vitality;
	}
	return nullptr;
}

AAbilityPawnBase* AAbilityModule::SpawnAbilityPawn(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FPawnSaveData>();
	if(AAbilityPawnBase* Pawn = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityPawnBase>({ &SaveData.ActorID, &SaveData.AssetID }, SaveData.GetItemData<UAbilityPawnDataBase>().Class))
	{
		Pawn->LoadSaveData(InSaveData);
		Pawn->SpawnDefaultController();
		if(InContainer)
		{
			InContainer->AddSceneActor(Pawn);
		}
		return Pawn;
	}
	return nullptr;
}

AAbilityCharacterBase* AAbilityModule::SpawnAbilityCharacter(FSaveData* InSaveData, ISceneContainerInterface* InContainer)
{
	auto& SaveData = InSaveData->CastRef<FCharacterSaveData>();
	if(AAbilityCharacterBase* Character = UObjectPoolModuleBPLibrary::SpawnObject<AAbilityCharacterBase>({ &SaveData.ActorID, &SaveData.AssetID }, SaveData.GetItemData<UAbilityCharacterDataBase>().Class))
	{
		Character->LoadSaveData(InSaveData);
		Character->SpawnDefaultController();
		if(InContainer)
		{
			InContainer->AddSceneActor(Character);
		}
		return Character;
	}
	return nullptr;
}
