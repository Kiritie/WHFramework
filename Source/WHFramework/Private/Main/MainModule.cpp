// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Ability/AbilityModule.h"
#include "AI/AIModule.h"
#include "Audio/AudioModule.h"
#include "Asset/AssetModule.h"
#include "Character/CharacterModule.h"
#include "Camera/CameraModule.h"
#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"
#include "FSM/FSMModule.h"
#include "Global/GlobalBPLibrary.h"
#include "Input/InputModule.h"
#include "LatentAction/LatentActionModule.h"
#include "Media/MediaModule.h"
#include "Network/NetworkModule.h"
#include "ReferencePool/ReferencePoolModule.h"
#include "Parameter/ParameterModule.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "WebRequest/WebRequestModule.h"
#include "Net/UnrealNetwork.h"
#include "Procedure/ProcedureModule.h"
#include "SaveGame/SaveGameModule.h"
#include "Scene/SceneModule.h"
#include "Step/StepModule.h"
#include "Voxel/VoxelModule.h"
#include "Widget/WidgetModule.h"

IMPLEMENTATION_MAIN_MODULE(AMainModule)

// ParamSets default values
AMainModule::AMainModule()
{
	ModuleName = FName("MainModule");
	
	ModuleClasses = TArray<TSubclassOf<AModuleBase>>();
	ModuleClasses.Add(AAbilityModule::StaticClass());
	ModuleClasses.Add(AAIModule::StaticClass());
	ModuleClasses.Add(AAssetModule::StaticClass());
	ModuleClasses.Add(AAudioModule::StaticClass());
	ModuleClasses.Add(ACharacterModule::StaticClass());
	ModuleClasses.Add(ACameraModule::StaticClass());
	ModuleClasses.Add(ADebugModule::StaticClass());
	ModuleClasses.Add(AEventModule::StaticClass());
	ModuleClasses.Add(AFSMModule::StaticClass());
	ModuleClasses.Add(AInputModule::StaticClass());
	ModuleClasses.Add(ALatentActionModule::StaticClass());
	ModuleClasses.Add(AMediaModule::StaticClass());
	ModuleClasses.Add(ANetworkModule::StaticClass());
	ModuleClasses.Add(AObjectPoolModule::StaticClass());
	ModuleClasses.Add(AParameterModule::StaticClass());
	ModuleClasses.Add(AProcedureModule::StaticClass());
	ModuleClasses.Add(AReferencePoolModule::StaticClass());
	ModuleClasses.Add(ASaveGameModule::StaticClass());
	ModuleClasses.Add(ASceneModule::StaticClass());
	ModuleClasses.Add(AStepModule::StaticClass());
	ModuleClasses.Add(AVoxelModule::StaticClass());
	ModuleClasses.Add(AWebRequestModule::StaticClass());
	ModuleClasses.Add(AWidgetModule::StaticClass());
	
	ModuleRefs = TArray<TScriptInterface<IModule>>();

	ModuleMap = TMap<FName, TScriptInterface<IModule>>();
}

AMainModule::~AMainModule()
{
	TERMINATION_MAIN_MODULE(AMainModule)
}

#if WITH_EDITOR
void AMainModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AMainModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void AMainModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_OnInitialize(ModuleRefs[i].GetObject());
		}
	}
}

void AMainModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			if(InPhase != EPhase::Final)
			{
				ModuleRefs[i]->Execute_OnPreparatory(ModuleRefs[i].GetObject(), InPhase);
			}
			else if(ModuleRefs[i]->Execute_IsAutoRunModule(ModuleRefs[i].GetObject()))
			{
				ModuleRefs[i]->Execute_Run(ModuleRefs[i].GetObject());
			}
		}
	}
}

void AMainModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i]->Execute_GetModuleState(ModuleRefs[i].GetObject()) == EModuleState::Running)
		{
			ModuleRefs[i]->Execute_OnRefresh(ModuleRefs[i].GetObject(), DeltaSeconds);
		}
	}
}

void AMainModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AMainModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AMainModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_Termination(ModuleRefs[i].GetObject());
		}
	}
	ModuleMap.Empty();
}

#if WITH_EDITOR
void AMainModule::GenerateModules_Implementation()
{
	// 获取场景模块
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);
	for(auto Iter : ChildActors)
	{
		if(const IModule* Module = Cast<IModule>(Iter))
		{
			const FName Name = Module->Execute_GetModuleName(Iter);
			Iter->SetActorLabel(Name.ToString());
			ModuleRefs.AddUnique(Iter);
			ModuleMap.Emplace(Name, Iter);
		}
	}

	// 移除废弃模块
	TArray<TScriptInterface<IModule>> RemoveList;
	for(auto Iter : ModuleRefs)
	{
		if(!Iter.GetObject() || !ModuleClasses.Contains(Iter.GetObject()->GetClass()))
		{
			RemoveList.AddUnique(Iter);
			if(AActor* Actor = Cast<AActor>(Iter.GetObject()))
			{
				Actor->Destroy();
			}
		}
	}
	for(auto Iter : RemoveList)
	{
		ModuleRefs.Remove(Iter);
		if(ModuleMap.Contains(Iter->Execute_GetModuleName(Iter.GetObject())))
		{
			ModuleMap.Remove(Iter->Execute_GetModuleName(Iter.GetObject()));
		}
	}

	// 生成新的模块
	for(int32 i = 0; i < ModuleClasses.Num(); i++)
	{
		if(!ModuleClasses[i]) continue;

		bool IsNeedSpawn = true;
		for(auto Iter : ModuleRefs)
		{
			if(Iter.GetObject() && Iter.GetObject()->IsA(ModuleClasses[i]))
			{
				Iter->Execute_OnGenerate(Iter.GetObject());
				ModuleMap.Emplace(Iter->Execute_GetModuleName(Iter.GetObject()), Iter.GetObject());
				IsNeedSpawn = false;
				break;
			}
		}
		if(!IsNeedSpawn) continue;
		
		FActorSpawnParameters ActorSpawnParameters = FActorSpawnParameters();
		ActorSpawnParameters.Owner = this;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		if(AModuleBase* Module = GetWorld()->SpawnActor<AModuleBase>(ModuleClasses[i], ActorSpawnParameters))
		{
			const FName Name = Module->Execute_GetModuleName(Module);
			Module->SetActorLabel(Name.ToString());
			Module->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
			Module->Execute_OnGenerate(Module);
			ModuleRefs.AddUnique(Module);
			ModuleMap.Emplace(Name, Module);
		}
	}

	Modify();
}

void AMainModule::DestroyModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_OnDestroy(ModuleRefs[i].GetObject());
			if(AActor* Actor = Cast<AActor>(ModuleRefs[i].GetObject()))
			{
				Actor->Destroy();
			}
		}
	}
	ModuleRefs.Empty();
	ModuleMap.Empty();
	Modify();
}
#endif

void AMainModule::PauseModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_Pause(ModuleRefs[i].GetObject());
		}
	}
}

void AMainModule::UnPauseModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_UnPause(ModuleRefs[i].GetObject());
		}
	}
}

UModuleNetworkComponent* AMainModule::GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponent> InModuleNetworkComponentClass, bool bInEditor)
{
	if(Get(bInEditor) && Get(bInEditor)->IsValidLowLevel())
	{
		if(AWHPlayerController* PlayerController = UGlobalBPLibrary::GetPlayerController<AWHPlayerController>())
		{
			return PlayerController->GetModuleNetCompByClass(InModuleNetworkComponentClass);
		}
	}
	return nullptr;
}

void AMainModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainModule, ModuleRefs);
}
