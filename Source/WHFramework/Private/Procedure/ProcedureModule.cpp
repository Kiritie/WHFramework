// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterModuleBPLibrary.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Event/EventModuleBPLibrary.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

// ParamSets default values
AProcedureModule::AProcedureModule()
{
	ModuleName = FName("ProcedureModule");

	Procedures = TArray<UProcedureBase*>();
	ProcedureMap = TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>();

	bAutoSwitch = false;

	FirstProcedure = nullptr;
	CurrentProcedure = nullptr;
}

#if WITH_EDITOR
void AProcedureModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void AProcedureModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();

	ClearAllProcedure();
}
#endif

void AProcedureModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	if(!FirstProcedure && Procedures.Num() > 0)
	{
		FirstProcedure = Procedures[0];
	}
	for(auto Iter : Procedures)
	{
		if(Iter)
		{
			Iter->OnInitialize();
		}
	}
}

void AProcedureModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	if(bAutoSwitch && FirstProcedure)
	{
		SwitchProcedure(FirstProcedure);
	}
}

void AProcedureModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(CurrentProcedure)
	{
		if(CurrentProcedure->GetProcedureState() != EProcedureState::Leaved)
		{
			CurrentProcedure->OnRefresh();
		}
	}
}

void AProcedureModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AProcedureModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AProcedureModule::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();
}

void AProcedureModule::SwitchProcedure(UProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure->GetProcedureState() != EProcedureState::Entered)
	{
		if(CurrentProcedure)
		{
			CurrentProcedure->OnLeave(InProcedure);
		}
		InProcedure->OnEnter(CurrentProcedure);

		CurrentProcedure = InProcedure;
	}
}

void AProcedureModule::SwitchProcedureByIndex(int32 InProcedureIndex)
{
	if(HasProcedureByIndex(InProcedureIndex))
	{
		SwitchProcedure(GetProcedureByIndex<UProcedureBase>(InProcedureIndex));
	}
	else
	{
		WHLog(WH_Procedure, Warning, TEXT("切换流程失败，不存在指定索引的流程: %d"), InProcedureIndex);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("切换流程失败，不存在指定索引的流程: %d"), InProcedureIndex));
	}
}

void AProcedureModule::SwitchProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass)
{
	if(HasProcedureByClass<UProcedureBase>(InProcedureClass))
	{
		SwitchProcedure(GetProcedureByClass<UProcedureBase>(InProcedureClass));
	}
	else
	{
		WHLog(WH_Procedure, Warning, TEXT("切换流程失败，不存在指定类型的流程: %s"), InProcedureClass ? *InProcedureClass->GetName() : TEXT("None"));
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("切换流程失败，不存在指定类型的流程: %s"), InProcedureClass ? *InProcedureClass->GetName() : TEXT("None")));
	}
}

void AProcedureModule::SwitchFirstProcedure()
{
	if(FirstProcedure)
	{
		SwitchProcedure(FirstProcedure);
	}
}

void AProcedureModule::SwitchLastProcedure()
{
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex > 0)
	{
		SwitchProcedureByIndex(CurrentProcedure->ProcedureIndex - 1);
	}
}

void AProcedureModule::SwitchNextProcedure()
{
	if(CurrentProcedure && CurrentProcedure->ProcedureIndex < Procedures.Num() - 1)
	{
		SwitchProcedureByIndex(CurrentProcedure->ProcedureIndex + 1);
	}
}

void AProcedureModule::GuideCurrentProcedure()
{
	if(CurrentProcedure)
	{
		CurrentProcedure->Guide();
	}
}

void AProcedureModule::ClearAllProcedure()
{
	for(auto Iter : Procedures)
	{
		if(Iter)
		{
			#if(WITH_EDITOR)
			Iter->OnUnGenerate();
			#endif
			Iter->ConditionalBeginDestroy();
		}
	}
	
	Procedures.Empty();

	Modify();
}

UProcedureBase* AProcedureModule::K2_GetProcedureByIndex(int32 InProcedureIndex, TSubclassOf<UProcedureBase> InProcedureClass) const
{
	return GetProcedureByIndex<UProcedureBase>(InProcedureIndex);
}

bool AProcedureModule::K2_HasProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass) const
{
	return HasProcedureByClass<UProcedureBase>(InProcedureClass);
}

UProcedureBase* AProcedureModule::K2_GetProcedureByClass(TSubclassOf<UProcedureBase> InProcedureClass) const
{
	return GetProcedureByClass<UProcedureBase>(InProcedureClass);
}

#if WITH_EDITOR

void AProcedureModule::GenerateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		Procedures[i]->GenerateListItem(Item);
		OutProcedureListItems.Add(Item);
	}
}

void AProcedureModule::UpdateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	for (int32 i = 0; i < Procedures.Num(); i++)
	{
		Procedures[i]->ProcedureIndex = i;
		Procedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}

void AProcedureModule::SetProcedureItem(int32 InIndex, UProcedureBase* InProcedure)
{
	if(Procedures.IsValidIndex(InIndex))
	{
		Procedures[InIndex] = InProcedure;
	}
}
#endif
