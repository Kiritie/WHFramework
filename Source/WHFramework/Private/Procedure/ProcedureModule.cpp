// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/ProcedureModule.h"

#include "Main/MainModule.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Base/RootProcedureBase.h"
#include "Net/UnrealNetwork.h"
#include "Character/CharacterModuleBPLibrary.h"
#include "Character/CharacterModuleTypes.h"
#include "Event/EventModule.h"
#include "Network/NetworkModuleBPLibrary.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Procedure/ProcedureModuleNetworkComponent.h"
#include "Scene/SceneModule.h"
#include "Scene/SceneModuleBPLibrary.h"

// ParamSets default values
AProcedureModule::AProcedureModule()
{
	ModuleName = FName("ProcedureModule");

	RootProcedures = TArray<ARootProcedureBase*>();

	RootProcedureIndex = 0;
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

	for(auto Iter : RootProcedures)
	{
		if(Iter)
		{
			Iter->NativeOnInitialize();
		}
	}

	//ServerStartProcedure();
}

void AProcedureModule::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();
}

void AProcedureModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);

	if(HasAuthority())
	{
		if(CurrentProcedure && CurrentProcedure->GetProcedureState() == EProcedureState::Entered)
		{
			CurrentProcedure->ServerOnRefresh(DeltaSeconds);
		}
	}
	else
	{
		if(CurrentLocalProcedure && CurrentLocalProcedure->GetLocalProcedureState() == EProcedureState::Entered)
		{
			CurrentLocalProcedure->LocalOnRefresh(DeltaSeconds);
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

void AProcedureModule::ServerStartProcedure()
{
	if(HasAuthority() && !bAllProcedureSpawned)
	{
		SpawnAllProcedure();
	}
	if(HasAuthority() && RootProcedures.IsValidIndex(RootProcedureIndex))
	{
		if(RootProcedures[RootProcedureIndex])
		{
			RootProcedures[RootProcedureIndex]->ServerPrepare();
		}
	}
}

void AProcedureModule::ServerEnterProcedure(AProcedureBase* InProcedure)
{
	if(!HasAuthority())
	{
		if(UProcedureModuleNetworkComponent* ProcedureModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UProcedureModuleNetworkComponent>())
		{
			ProcedureModuleNetworkComponent->ServerEnterProcedure(InProcedure);
		}
		return;
	}
	
	if(InProcedure && InProcedure != CurrentProcedure && InProcedure->GetProcedureState() != EProcedureState::Entered)
	{
		if(CurrentProcedure && CurrentProcedure != InProcedure->ParentProcedure)
		{
			CurrentProcedure->ServerOnLeave(InProcedure);
		}
		InProcedure->ServerOnEnter(CurrentProcedure);
		CurrentProcedure = InProcedure;

		if(CurrentProcedure->HasSubProcedure(false))
		{
			CurrentParentProcedure = CurrentProcedure;
		}

		if(CurrentProcedure->IsA(ARootProcedureBase::StaticClass()))
		{
			RootProcedureIndex = CurrentProcedure->ProcedureIndex;
		}
	}
}

void AProcedureModule::ServerLeaveProcedure(AProcedureBase* InProcedure)
{
	if(!HasAuthority())
	{
		if(UProcedureModuleNetworkComponent* ProcedureModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UProcedureModuleNetworkComponent>())
		{
			ProcedureModuleNetworkComponent->ServerLeaveProcedure(InProcedure);
		}
		return;
	}
	if(InProcedure && (InProcedure == CurrentProcedure || InProcedure == CurrentProcedure->ParentProcedure) && InProcedure->GetProcedureState() != EProcedureState::Leaved)
	{
		if(InProcedure->ParentProcedure)
		{
			CurrentProcedure->ServerOnLeave(InProcedure->ParentProcedure);
			CurrentProcedure = InProcedure->ParentProcedure;
		}
		else
		{
			CurrentProcedure->ServerOnLeave(nullptr);
		}

		if(CurrentProcedure->IsA(ARootProcedureBase::StaticClass()))
		{
			const int32 Index = RootProcedureIndex + 1;
			if(RootProcedures.IsValidIndex(Index))
			{
				if(RootProcedures[Index])
				{
					RootProcedures[Index]->ServerPrepare();
				}
			}
		}

		Execute_Pause(this);
	}
}

void AProcedureModule::LocalEnterProcedure(AProcedureBase* InProcedure)
{
	if(InProcedure && InProcedure != CurrentLocalProcedure && InProcedure->GetLocalProcedureState() != EProcedureState::Entered)
	{
		if(CurrentLocalProcedure && CurrentLocalProcedure != InProcedure->ParentProcedure)
		{
			CurrentLocalProcedure->LocalOnLeave(InProcedure);
		}
		InProcedure->LocalOnEnter(CurrentLocalProcedure);
		CurrentLocalProcedure = InProcedure;
	}
}

void AProcedureModule::LocalLeaveProcedure(AProcedureBase* InProcedure)
{
	if(InProcedure && (InProcedure == CurrentLocalProcedure || InProcedure == CurrentLocalProcedure->ParentProcedure) && InProcedure->GetLocalProcedureState() != EProcedureState::Leaved)
	{
		if(InProcedure->ParentProcedure)
		{
			CurrentLocalProcedure->LocalOnLeave(InProcedure->ParentProcedure);
			CurrentLocalProcedure = InProcedure->ParentProcedure;
		}
		else
		{
			CurrentLocalProcedure->LocalOnLeave(nullptr);
		}
	}
}

void AProcedureModule::ServerSkipProcedure(AProcedureBase* InProcedure)
{
	if(!InProcedure) return;
	
	if(!HasAuthority())
	{
		if(UProcedureModuleNetworkComponent* ProcedureModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UProcedureModuleNetworkComponent>())
		{
			ProcedureModuleNetworkComponent->ServerSkipProcedure(InProcedure);
		}
		return;
	}
	switch(InProcedure->GetProcedureState())
	{
		case EProcedureState::Entered:
		{
			InProcedure->ServerComplete(EProcedureExecuteResult::Skipped);
			break;
		}
		case EProcedureState::Completed:
		{
			ServerLeaveProcedure(InProcedure);
			break;
		}
		default: break;
	}
}

void AProcedureModule::ServerSkipCurrentProcedure()
{
	ServerSkipProcedure(CurrentProcedure);
}

void AProcedureModule::ServerSkipCurrentParentProcedure()
{
	ServerSkipProcedure(CurrentParentProcedure);
}

void AProcedureModule::ServerSkipCurrentRootProcedure()
{
	ServerSkipProcedure(GetCurrentRootProcedure());
}

void AProcedureModule::SpawnAllProcedure()
{
	if(HasRootProcedure())
	{
		for (int32 i = 0; i < RootProcedures.Num(); i++)
		{
			if(RootProcedures[i])
			{
				FActorSpawnParameters ActorSpawnParameters = FActorSpawnParameters();
				ActorSpawnParameters.Owner = this;
				ActorSpawnParameters.Template = RootProcedures[i];
				ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				if(ARootProcedureBase* RootProcedure = GetWorld()->SpawnActor<ARootProcedureBase>(RootProcedures[i]->GetClass(), ActorSpawnParameters))
				{
					#if WITH_EDITOR
					RootProcedure->SetActorLabel(RootProcedures[i]->ProcedureName.ToString());
					#endif
					RootProcedure->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
					RootProcedure->ServerOnInitialize();
					RootProcedures[i] = RootProcedure;
				}
			}
		}
		bAllProcedureSpawned = true;
	}
}

void AProcedureModule::ClearAllProcedure()
{
	for(auto Iter : RootProcedures)
	{
		if(Iter)
		{
			#if(WITH_EDITOR)
			Iter->OnUnGenerate();
			#endif
			Iter->ConditionalBeginDestroy();
		}
	}
	
	RootProcedures.Empty();

	Modify();
}

#if WITH_EDITOR
void AProcedureModule::OpenProcedureEditor()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("WHProcedureEditor"));
}

void AProcedureModule::GenerateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	OutProcedureListItems = TArray<TSharedPtr<FProcedureListItem>>();
	for (int32 i = 0; i < RootProcedures.Num(); i++)
	{
		auto Item = MakeShared<FProcedureListItem>();
		RootProcedures[i]->GenerateListItem(Item);
		OutProcedureListItems.Add(Item);
	}
}

void AProcedureModule::UpdateListItem(TArray<TSharedPtr<FProcedureListItem>>& OutProcedureListItems)
{
	for (int32 i = 0; i < RootProcedures.Num(); i++)
	{
		RootProcedures[i]->ProcedureIndex = i;
		RootProcedures[i]->UpdateListItem(OutProcedureListItems[i]);
	}
}
#endif

bool AProcedureModule::HasRootProcedure() const
{
	return RootProcedures.Num() > 0;
}

ARootProcedureBase* AProcedureModule::GetCurrentRootProcedure() const
{
	if(RootProcedures.IsValidIndex(RootProcedureIndex))
	{
		return RootProcedures[RootProcedureIndex];
	}
	return nullptr;
}

void AProcedureModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(AProcedureModule, CurrentProcedure);
	DOREPLIFETIME(AProcedureModule, CurrentLocalProcedure);
	DOREPLIFETIME(AProcedureModule, RootProcedureIndex);
	DOREPLIFETIME(AProcedureModule, RootProcedures);
}
