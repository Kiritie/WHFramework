// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Ability/AbilityModule.h"
#include "Audio/AudioModule.h"
#include "Asset/AssetModule.h"
#include "Main/Base/ModuleBase.h"
#include "Character/CharacterModule.h"
#include "Camera/CameraModule.h"
#include "Debug/DebugModule.h"
#include "Debug/DebugModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"
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

AMainModule* AMainModule::Instance = nullptr;
#if WITH_EDITOR
AMainModule* AMainModule::InstanceEditor = nullptr;
#endif
// ParamSets default values
AMainModule::AMainModule()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ModuleClasses = TArray<TSubclassOf<AModuleBase>>();
	ModuleClasses.Add(AAbilityModule::StaticClass());
	ModuleClasses.Add(AAssetModule::StaticClass());
	ModuleClasses.Add(AAudioModule::StaticClass());
	ModuleClasses.Add(ACharacterModule::StaticClass());
	ModuleClasses.Add(ACameraModule::StaticClass());
	ModuleClasses.Add(ADebugModule::StaticClass());
	ModuleClasses.Add(AEventModule::StaticClass());
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
	if(Instance == this)
	{
		Instance = nullptr;
	}
	#if WITH_EDITOR
	if(InstanceEditor == this)
	{
		InstanceEditor = nullptr;
	}
	#endif
}

// Called when the game starts or when spawned
void AMainModule::BeginPlay()
{
	Super::BeginPlay();

	PreparatoryModules();
}

void AMainModule::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	TerminationModules();
}

// Called every frame
void AMainModule::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshModules(DeltaTime);
}

AMainModule* AMainModule::Get(bool bInEditor)
{
	#if WITH_EDITOR
	if(!bInEditor)
	{
	#endif
		if(!Instance || !Instance->IsValidLowLevel())
		{
			Instance = UGlobalBPLibrary::GetObjectInExistedWorld<AMainModule>([](const UWorld* World) {
				return UGameplayStatics::GetActorOfClass(World, AMainModule::StaticClass());
			}, false);
		}
		return Instance;
	#if WITH_EDITOR
	}
	else
	{
		if(!InstanceEditor || !InstanceEditor->IsValidLowLevel())
		{
			InstanceEditor = UGlobalBPLibrary::GetObjectInExistedWorld<AMainModule>([](const UWorld* World) {
				return UGameplayStatics::GetActorOfClass(World, AMainModule::StaticClass());
			}, true);
		}
		return InstanceEditor;
	}
	#endif
}

#if WITH_EDITOR
void AMainModule::GenerateModules_Implementation()
{
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);
	for(auto Iter : ChildActors)
	{
		if(const IModule* Module = Cast<IModule>(Iter))
		{
			if(!ModuleRefs.Contains(Iter))
			{
				ModuleRefs.Add(Iter);
			}
			const FName ModuleName = Module->Execute_GetModuleName(Iter);
			if(!ModuleMap.Contains(ModuleName))
			{
				ModuleMap.Add(ModuleName, Iter);
			}
			else if(!ModuleMap[ModuleName])
			{
				ModuleMap[ModuleName] = Iter;
			}
		}
	}

	for(int32 i = 0; i < ModuleClasses.Num(); i++)
	{
		if(!ModuleClasses[i]) continue;

		bool IsNeedSpawn = true;
		for(auto Iter : ModuleRefs)
		{
			if(Iter.GetObject() && Iter.GetObject()->IsA(ModuleClasses[i]))
			{
				if(!ModuleMap.Contains(Iter->Execute_GetModuleName(Iter.GetObject())))
				{
					ModuleMap.Add(Iter->Execute_GetModuleName(Iter.GetObject()), Iter.GetObject());
				}
				Iter->Execute_OnGenerate(Iter.GetObject());
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
			ModuleRefs.Add(Module);
			if(!ModuleMap.Contains(Module->Execute_GetModuleName(Module)))
			{
				ModuleMap.Add(Module->Execute_GetModuleName(Module), Module);
			}
			Module->SetActorLabel(Module->Execute_GetModuleName(Module).ToString());
			Module->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
			Module->Execute_OnGenerate(Module);
		}
	}

	TArray<TScriptInterface<IModule>> RemoveList;

	for(auto Iter : ModuleRefs)
	{
		if(!Iter.GetObject() || !ModuleClasses.Contains(Iter.GetObject()->GetClass()))
		{
			RemoveList.Add(Iter);
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

void AMainModule::InitializeModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_OnInitialize(ModuleRefs[i].GetObject());
		}
	}
}

void AMainModule::PreparatoryModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_OnPreparatory(ModuleRefs[i].GetObject());
		}
	}
}

void AMainModule::RefreshModules_Implementation(float DeltaSeconds)
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i]->Execute_GetModuleState(ModuleRefs[i].GetObject()) == EModuleState::Running)
		{
			ModuleRefs[i]->Execute_OnRefresh(ModuleRefs[i].GetObject(), DeltaSeconds);
		}
	}
}

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

void AMainModule::TerminationModules_Implementation()
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel())
		{
			ModuleRefs[i]->Execute_OnTermination(ModuleRefs[i].GetObject());
		}
	}
	ModuleMap.Empty();
}

void AMainModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainModule, ModuleRefs);
}
