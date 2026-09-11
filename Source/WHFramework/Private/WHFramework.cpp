// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFramework.h"
#include "Ability/AbilityModuleTypes.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Item/Widget/WidgetAbilityDragItemBase.h"
#include "Ability/Projectile/AbilityProjectileBase.h"
#include "Ability/Spawner/AbilitySpawnerBase.h"
#include "Character/Base/CharacterBase.h"
#include "Common/Base/WHActor.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Pawn/Base/PawnBase.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/Widget/Item/WidgetEnumSettingItemBase.h"
#include "Setting/Widget/Item/WidgetFloatSettingItemBase.h"
#include "Setting/Widget/Item/WidgetKeySettingItemBase.h"
#include "Setting/Widget/Item/WidgetOptionSettingItemBase.h"
#include "Setting/Widget/Item/WidgetSettingItemBase.h"
#include "Setting/Widget/Item/WidgetSettingItemCategoryBase.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "UObject/CoreRedirects.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkModule"

void FWHFrameworkModule::StartupModule()
{
	TArray<FCoreRedirect> Redirects;
	const TPair<const TCHAR*, const TCHAR*> Classes[] = {
		{TEXT("Dialogue"), TEXT("DialogueAsset")},
		{TEXT("DialogueEvents"), TEXT("DialogueEventBase")},
		{TEXT("DialogueConditions"), TEXT("DialogueConditionBase")},
		{TEXT("OrCondition"), TEXT("DialogueCondition_Or")},
		{TEXT("AndCondition"), TEXT("DialogueCondition_And")},
		{TEXT("DialogueUserWidget"), TEXT("WidgetDialogueBase")}
	};
	for (const auto& Class : Classes)
	{
		const FString Destination = FString(TEXT("/Script/WHFramework.")) + Class.Value;
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString(TEXT("/Script/DialoguePlugin.")) + Class.Key, Destination);
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString(TEXT("/Script/WHFramework.")) + Class.Key, Destination);
	}
	Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/DialoguePlugin.DialogueNode"), TEXT("/Script/WHFramework.DialogueNode"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/DialogueSoundConcurrency"), TEXT("/WHFramework/Dialogue/DialogueSoundConcurrency"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/UI/DemoDialogueWidget"), TEXT("/WHFramework/Dialogue/UI/DemoDialogueWidget"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/UI/DemoReplyWidget"), TEXT("/WHFramework/Dialogue/UI/DemoReplyWidget"));
	FCoreRedirects::AddRedirectList(Redirects, TEXT("WHFrameworkDialogueMigration"));

	FObjectPoolSpawnParameterRegistry::Register(AWHActor::StaticClass(), FWHActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilityActorBase::StaticClass(), FAbilityActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilityProjectileBase::StaticClass(), FAbilityProjectileSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilityItemBase::StaticClass(), FAbilityItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilitySpawnerBase::StaticClass(), FWHActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(ACharacterBase::StaticClass(), FAbilityActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(APawnBase::StaticClass(), FAbilityActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetAbilityDragItemBase::StaticClass(), FAbilityWidgetSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetSettingItemBase::StaticClass(), FWidgetSettingItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetFloatSettingItemBase::StaticClass(), FWidgetFloatSettingItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetEnumSettingItemBase::StaticClass(), FWidgetEnumSettingItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetOptionSettingItemBase::StaticClass(), FWidgetOptionSettingItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetKeySettingItemBase::StaticClass(), FWidgetKeySettingItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UWidgetSettingItemCategoryBase::StaticClass(), FWidgetSettingItemCategorySpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UVoxelMeshComponent::StaticClass(), FVoxelMeshComponentSpawnParameter::StaticStruct());
}

void FWHFrameworkModule::ShutdownModule()
{
	FObjectPoolSpawnParameterRegistry::Clear();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWHFrameworkModule, WHFramework)
