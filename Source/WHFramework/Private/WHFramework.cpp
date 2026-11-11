// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFramework.h"
#include "Ability/AbilityActorTypes.h"
#include "Ability/AbilityItemTypes.h"
#include "Widget/WidgetMountTypes.h"
#include "Ability/Actor/AbilityActorBase.h"
#include "Ability/Item/AbilityItemBase.h"
#include "Ability/Spawner/AbilitySpawnerBase.h"
#include "Character/Base/CharacterBase.h"
#include "Common/Base/WHActor.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Pawn/Base/PawnBase.h"
#include "SaveGame/SaveGameAsyncExecutor.h"
#include "Voxel/Components/VoxelMeshComponent.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Voxel/Save/InventoryVoxelEntityCodec.h"
#include "Widget/Common/CommonOptionSelector.h"
#include "Widget/WidgetModuleTypes.h"
#include "UObject/CoreRedirects.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkModule"

void FWHFrameworkModule::StartupModule()
{
	if (!FInventoryVoxelEntityCodec::Register())
	{
		UE_LOG(LogTemp, Fatal, TEXT("WHFramework inventory entity codec registration failed"));
	}
	TArray<FCoreRedirect> Redirects;
	const TPair<const TCHAR*, const TCHAR*> Classes[] = {{TEXT("Dialogue"), TEXT("DialogueAsset")},
	                                                     {TEXT("DialogueEvents"), TEXT("DialogueEventBase")},
	                                                     {TEXT("DialogueConditions"), TEXT("DialogueConditionBase")},
	                                                     {TEXT("OrCondition"), TEXT("DialogueCondition_Or")},
	                                                     {TEXT("AndCondition"), TEXT("DialogueCondition_And")},
	                                                     {TEXT("DialogueUserWidget"), TEXT("WidgetDialogueBase")}};
	for (const auto& Class : Classes)
	{
		const FString Destination = FString(TEXT("/Script/WHFramework.")) + Class.Value;
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString(TEXT("/Script/DialoguePlugin.")) + Class.Key, Destination);
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString(TEXT("/Script/WHFramework.")) + Class.Key, Destination);
	}
	Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/DialoguePlugin.DialogueNode"), TEXT("/Script/WHFramework.DialogueNode"));
	Redirects.Emplace(
	    ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/DialogueSoundConcurrency"), TEXT("/WHFramework/Sounds/Dialogue/DialogueSoundConcurrency"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/UI/DemoDialogueWidget"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_Dialogue_Demo"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/UI/DemoReplyWidget"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_DialogueReply_Demo"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Object, TEXT("/DialoguePlugin/UI/DemoDialogueWidget.DemoDialogueWidget"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_Dialogue_Demo.WBP_Dialogue_Demo"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/DialoguePlugin/UI/DemoDialogueWidget.DemoDialogueWidget_C"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_Dialogue_Demo.WBP_Dialogue_Demo_C"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Object, TEXT("/DialoguePlugin/UI/DemoDialogueWidget.DemoDialogueWidget_C"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_Dialogue_Demo.WBP_Dialogue_Demo_C"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Object, TEXT("/DialoguePlugin/UI/DemoReplyWidget.DemoReplyWidget"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_DialogueReply_Demo.WBP_DialogueReply_Demo"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Class, TEXT("/DialoguePlugin/UI/DemoReplyWidget.DemoReplyWidget_C"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_DialogueReply_Demo.WBP_DialogueReply_Demo_C"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Object, TEXT("/DialoguePlugin/UI/DemoReplyWidget.DemoReplyWidget_C"), TEXT("/WHFramework/Blueprints/Widget/Dialogue/WBP_DialogueReply_Demo.WBP_DialogueReply_Demo_C"));
	FCoreRedirects::AddRedirectList(Redirects, TEXT("WHFrameworkDialogueMigration"));

	FObjectPoolSpawnParameterRegistry::Register(AWHActor::StaticClass(), FWHActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilityActorBase::StaticClass(), FAbilityActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilityItemBase::StaticClass(), FAbilityItemSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(AAbilitySpawnerBase::StaticClass(), FWHActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(ACharacterBase::StaticClass(), FAbilityActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(APawnBase::StaticClass(), FAbilityActorSpawnParameter::StaticStruct());
	FObjectPoolSpawnParameterRegistry::Register(UCommonOptionSelector::StaticClass(), FCommonOptionSelectorSpawnParameter::StaticStruct());
}

void FWHFrameworkModule::ShutdownModule()
{
	FSaveGameAsyncExecutor::DrainRetiredTasks();
	FObjectPoolSpawnParameterRegistry::Clear();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FWHFrameworkModule, WHFramework)
