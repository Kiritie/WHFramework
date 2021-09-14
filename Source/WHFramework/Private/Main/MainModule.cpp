// Fill out your copyright notice in the Description page of Project Settings.


#include "Main/MainModule.h"

#include "Asset/AssetModule.h"
#include "Base/ModuleBase.h"
#include "Event/EventModule.h"
#include "LatentAction/LatentActionModule.h"
#include "Network/NetworkModule.h"
#include "SpawnPool/SpawnPoolModule.h"
#include "Parameter/ParameterModule.h"
#include "ObjectPool/ObjectPoolModule.h"
#include "WebRequest/WebRequestModule.h"
#include "Net/UnrealNetwork.h"

// ParamSets default values
AMainModule::AMainModule()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));

	ModuleClasses = TArray<TSubclassOf<AModuleBase>>();
	
	ModuleRefs = TArray<TScriptInterface<IModule>>();
}

// Called when the game starts or when spawned
void AMainModule::BeginPlay()
{
	Super::BeginPlay();

	InitializeModules();
}

// Called every frame
void AMainModule::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	RefreshModules(DeltaTime);
}

#if WITH_EDITOR
void AMainModule::GenerateModules_Implementation()
{
	TArray<AActor*> ChildActors;
	GetAttachedActors(ChildActors);
	for(auto Iter : ChildActors)
	{
		if(Iter->GetClass()->ImplementsInterface(UModule::StaticClass()) && !ModuleRefs.Contains(Iter))
		{
			ModuleRefs.Add(Iter);
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
				Iter->Execute_OnGenerate(Iter.GetObject(), CourseMode);
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
			Module->SetActorLabel(ModuleClasses[i]->GetDefaultObject<AModuleBase>()->Execute_GetModuleName(ModuleClasses[i]->GetDefaultObject<AModuleBase>()).ToString());
			Module->AttachToActor(this, FAttachmentTransformRules::SnapToTargetIncludingScale);
			Module->Execute_OnGenerate(Module, CourseMode);
			ModuleRefs.Add(Module);
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

void AMainModule::RefreshModules_Implementation(float DeltaSeconds)
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i]->Execute_GetModuleState(ModuleRefs[i].GetObject()) == EModuleState::Executing)
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

AModuleBase* AMainModule::K2_GetModuleByClass(TSubclassOf<AModuleBase> InModuleClass)
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i].GetObject()->IsA(InModuleClass))
		{
			return Cast<AModuleBase>(ModuleRefs[i].GetObject());
		}
	}
	return nullptr;
}

TScriptInterface<IModule> AMainModule::K2_GetModuleByName(const FName InModuleName)
{
	for(int32 i = 0; i < ModuleRefs.Num(); i++)
	{
		if(ModuleRefs[i] && ModuleRefs[i].GetObject()->IsValidLowLevel() && ModuleRefs[i]->GetModuleName() == InModuleName)
		{
			return ModuleRefs[i];
		}
	}
	return nullptr;
}

void AMainModule::AddCourseLog(ECourseLogModuleType InModuleType, const FString& InModuleName, const FString& InModuleDesc, const FString& InJsonString)
{
	FString LogContent;

	LogContent += TEXT("时间: ");
	LogContent += FDateTime::Now().ToString();
	LogContent += TEXT("\t|\t");
	
	LogContent += TEXT("模块类型: ");
	switch (InModuleType)
	{
		case ECourseLogModuleType::Procedure :
		{
			LogContent += TEXT("流程");
			break;
		}

		case ECourseLogModuleType::Examination :
		{
			LogContent += TEXT("测试");
			break;
		}

		case ECourseLogModuleType::Listener :
		{
			LogContent += TEXT("监听");
			break;
		}
	}
	LogContent += TEXT("\t|\t");

	LogContent += TEXT("模块名称: ") + InModuleName;
	LogContent += InModuleName.Len() <= 10 ? TEXT("\t\t") : TEXT("\t");

	LogContent += TEXT("|\t");
	
	LogContent += TEXT("模块描述: ") + InModuleDesc;
	LogContent += TEXT("\n");

	FFileHelper::SaveStringToFile(LogContent , *CourseLogFilePath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), EFileWrite::FILEWRITE_Append);
}

void AMainModule::ClearCourseLog()
{
	FFileHelper::SaveStringToFile(TEXT("") , *CourseLogFilePath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), EFileWrite::FILEWRITE_NoFail);
}

void AMainModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AMainModule, ModuleRefs);
}
