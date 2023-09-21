// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Ability/AbilityModule.h"
#include "AI/AIModule.h"
#include "Audio/AudioModule.h"
#include "Asset/AssetModule.h"
#include "Character/CharacterModule.h"
#include "Camera/CameraModule.h"
#include "Debug/DebugModule.h"
#include "Event/EventModule.h"
#include "FSM/FSMModule.h"
#include "Common/CommonBPLibrary.h"
#include "Input/InputModule.h"
#include "Animation/AnimationModule.h"
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
#include "Task/TaskModule.h"
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
	ModuleClasses.Add(AAnimationModule::StaticClass());
	ModuleClasses.Add(AAssetModule::StaticClass());
	ModuleClasses.Add(AAudioModule::StaticClass());
	ModuleClasses.Add(ACharacterModule::StaticClass());
	ModuleClasses.Add(ACameraModule::StaticClass());
	ModuleClasses.Add(ADebugModule::StaticClass());
	ModuleClasses.Add(AEventModule::StaticClass());
	ModuleClasses.Add(AFSMModule::StaticClass());
	ModuleClasses.Add(AInputModule::StaticClass());
	ModuleClasses.Add(AMediaModule::StaticClass());
	ModuleClasses.Add(ANetworkModule::StaticClass());
	ModuleClasses.Add(AObjectPoolModule::StaticClass());
	ModuleClasses.Add(AParameterModule::StaticClass());
	ModuleClasses.Add(AProcedureModule::StaticClass());
	ModuleClasses.Add(AReferencePoolModule::StaticClass());
	ModuleClasses.Add(ASaveGameModule::StaticClass());
	ModuleClasses.Add(ASceneModule::StaticClass());
	ModuleClasses.Add(AStepModule::StaticClass());
	ModuleClasses.Add(ATaskModule::StaticClass());
	ModuleClasses.Add(AVoxelModule::StaticClass());
	ModuleClasses.Add(AWebRequestModule::StaticClass());
	ModuleClasses.Add(AWidgetModule::StaticClass());
	
	ModuleRefs = TArray<AModuleBase*>();

	ModuleMap = TMap<FName, AModuleBase*>();
}

AMainModule::~AMainModule()
{
	TERMINATION_MAIN_MODULE(AMainModule)
}

#if WITH_EDITOR
void AMainModule::OnGenerate()
{
	Super::OnGenerate();
}

void AMainModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AMainModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_OnInitialize(ModuleRefs[i]);
		}
	}
}

void AMainModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel())
		{
			if(InPhase != EPhase::Final)
			{
				ModuleRefs[i]->Execute_OnPreparatory(ModuleRefs[i], InPhase);
			}
			else if(ModuleRefs[i]->IsAutoRunModule())
			{
				ModuleRefs[i]->Run();
			}
		}
	}
}

void AMainModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel() && ModuleRefs[i]->GetModuleState() == EModuleState::Running)
		{
			ModuleRefs[i]->Execute_OnRefresh(ModuleRefs[i], DeltaSeconds);
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

void AMainModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel())
		{
			if(InPhase != EPhase::Final)
			{
				ModuleRefs[i]->Execute_OnTermination(ModuleRefs[i], InPhase);
			}
			else
			{
				ModuleRefs[i]->Termination();
			}
		}
	}
	if(PHASEC(InPhase, EPhase::Final))
	{
		ModuleMap.Empty();
	}
}

#if WITH_EDITOR
void AMainModule::GenerateModules()
{
	ModuleMap.Empty();

	// 移除废弃模块
	for(int32 i = ModuleRefs.Num() - 1; i >= 0; --i)
	{
		if(!ModuleRefs[i] || !ModuleClasses.Contains(ModuleRefs[i]->GetClass()))
		{
			if(ModuleRefs[i])
			{
				ModuleRefs[i]->Destroy();
			}
			ModuleRefs.RemoveAt(i);
		}
	}

	// 获取场景模块
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);
	for(auto Iter : ChildActors)
	{
		auto Module = Cast<AModuleBase>(Iter);
		if(ModuleClasses.Contains(Module->GetClass()))
		{
			const FName Name = Module->GetModuleName();
			Iter->SetActorLabel(Name.ToString());
			ModuleRefs.AddUnique(Module);
			ModuleMap.Emplace(Name, Module);
		}
		else
		{
			Module->Destroy();
		}
	}

	// 生成新的模块
	for(int32 i = 0; i < ModuleClasses.Num(); i++)
	{
		if(!ModuleClasses[i]) continue;

		bool IsNeedSpawn = true;
		for(auto Iter : ModuleRefs)
		{
			if(Iter && Iter->IsA(ModuleClasses[i]))
			{
				Iter->OnGenerate();
				const FName Name = Iter->GetModuleName();
				ModuleMap.Emplace(Name, Iter);
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
			const FName Name = Module->GetModuleName();
			Module->SetActorLabel(Name.ToString());
			Module->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
			Module->OnGenerate();
			ModuleRefs.AddUnique(Module);
			ModuleMap.Emplace(Name, Module);
		}
	}

	Modify();
}

void AMainModule::DestroyModules()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel())
		{
			ModuleRefs[i]->OnDestroy();
			if(AActor* Actor = Cast<AActor>(ModuleRefs[i]))
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
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel())
		{
			ModuleRefs[i]->Pause();
		}
	}
}

void AMainModule::UnPauseModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i]->IsValidLowLevel())
		{
			ModuleRefs[i]->UnPause();
		}
	}
}

UModuleNetworkComponentBase* AMainModule::GetModuleNetworkComponentByClass(TSubclassOf<UModuleNetworkComponentBase> InModuleNetworkComponentClass, bool bInEditor)
{
	if(Get(bInEditor) && Get(bInEditor)->IsValidLowLevel())
	{
		if(AWHPlayerController* PlayerController = UCommonBPLibrary::GetPlayerController<AWHPlayerController>())
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
