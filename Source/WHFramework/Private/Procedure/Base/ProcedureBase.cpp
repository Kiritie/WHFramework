// Fill out your copyright notice in the Description page of Project Settings.


#include "Procedure/Base/ProcedureBase.h"

#include "EventHandle_EnterProcedure.h"
#include "EventHandle_LeaveProcedure.h"
#include "Event/EventModuleBPLibrary.h"
#include "Main/MainModuleBPLibrary.h"
#include "Procedure/ProcedureModule.h"
#include "Procedure/ProcedureModuleBPLibrary.h"
#include "Net/UnrealNetwork.h"

AProcedureBase::AProcedureBase()
{
	bReplicates = true;

	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	bActorLabelEditable = false;
#endif

	ProcedureName = FName("ProcedureBase");
	ProcedureDisplayName = FText::FromString(TEXT("Procedure Base"));
	ProcedureDescription = FText::GetEmpty();

	ProcedureIndex = 0;
	ProcedureType = EProcedureType::Default;
	ProcedureState = EProcedureState::None;

	ProcedureExecuteType = EProcedureExecuteType::Inherit;
	ProcedureExecuteCondition = EProcedureExecuteResult::None;
	ProcedureGuideType = EProcedureGuideType::None;

	bAutoEnterProcedure = false;
	AutoEnterProcedureTime = 0.f;

	bAutoLeaveProcedure = false;
	AutoLeaveProcedureTime = 0.f;

	CurrentSubProcedureIndex = -1;

	CurrentProcedureTaskIndex = -1;

	bMergeSubProcedure = false;

	SubProcedures = TArray<AProcedureBase*>();

	ParentProcedure = nullptr;

	ProcedureState = EProcedureState::None;

	RootComponent = CreateDefaultSubobject<USceneComponent>(FName("RootComponent"));
}

#if WITH_EDITOR
void AProcedureBase::OnGenerate()
{
	
}

void AProcedureBase::OnUnGenerate()
{
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			Iter->OnUnGenerate();
			Iter->ConditionalBeginDestroy();
		}
	}
	SubProcedures.Empty();
}
#endif

void AProcedureBase::GenerateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
	for (int32 i = 0; i < SubProcedures.Num(); i++)
	{
		if(SubProcedures[i])
		{
			auto Item = MakeShared<FProcedureListItem>();
			Item->ParentListItem = OutProcedureListItem;
			OutProcedureListItem->SubListItems.Add(Item);
			SubProcedures[i]->GenerateListItem(Item);
		}
	}
}

void AProcedureBase::UpdateListItem(TSharedPtr<FProcedureListItem> OutProcedureListItem)
{
	OutProcedureListItem->Procedure = this;
	for (int32 i = 0; i < SubProcedures.Num(); i++)
	{
		if(SubProcedures[i])
		{
			SubProcedures[i]->ProcedureIndex = i;
			SubProcedures[i]->ParentProcedure = this;
			SubProcedures[i]->UpdateListItem(OutProcedureListItem->SubListItems[i]);
		}
	}
}

void AProcedureBase::NativeOnInitialize()
{
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			Iter->NativeOnInitialize();
		}
	}
}

void AProcedureBase::ServerOnInitialize_Implementation()
{
	for (int32 i = 0; i < SubProcedures.Num(); i++)
	{
		if(SubProcedures[i])
		{
			FActorSpawnParameters ActorSpawnParameters = FActorSpawnParameters();
			ActorSpawnParameters.Owner = this;
			ActorSpawnParameters.Template = SubProcedures[i];
			ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			if (AProcedureBase* ProcedureBase = GetWorld()->SpawnActor<AProcedureBase>(SubProcedures[i]->GetClass(), ActorSpawnParameters))
			{
				#if WITH_EDITOR
				ProcedureBase->SetActorLabel(SubProcedures[i]->ProcedureName.ToString());
				#endif
				ProcedureBase->ParentProcedure = this;
				ProcedureBase->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
				ProcedureBase->ServerOnInitialize();
				SubProcedures[i] = ProcedureBase;
			}
		}
	}

	MultiOnInitialize();

	ProcedureState = EProcedureState::Initialized;
	OnRep_ProcedureState();
}

void AProcedureBase::MultiOnInitialize_Implementation()
{
	if(LocalProcedureState != EProcedureState::Initialized)
	{
		LocalOnInitialize();
	}
}

void AProcedureBase::LocalOnInitialize_Implementation()
{
	LocalProcedureState = EProcedureState::Initialized;
	OnChangeLocalProcedureState(LocalProcedureState);
}

void AProcedureBase::ServerOnPrepare_Implementation()
{
	CurrentSubProcedureIndex = -1;
	CurrentProcedureTaskIndex = -1;

	for(int32 i = 0; i < ProcedureTaskItems.Num(); i++)
	{
		ProcedureTaskItems[i].TaskState = EProcedureTaskState::None;
	}
	
	MultiOnPrepare();

	ProcedureState = EProcedureState::Prepared;
	OnRep_ProcedureState();
}

void AProcedureBase::MultiOnPrepare_Implementation()
{
	if(LocalProcedureState != EProcedureState::Prepared)
	{
		LocalOnPrepare();
	}
}

void AProcedureBase::LocalOnPrepare_Implementation()
{
	LocalProcedureState = EProcedureState::Prepared;
	OnChangeLocalProcedureState(LocalProcedureState);
}

void AProcedureBase::ServerOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	GetWorld()->GetTimerManager().ClearTimer(AutoEnterTimerHandle);
	
	if (ParentProcedure)
	{
		ParentProcedure->CurrentSubProcedureIndex = ProcedureIndex;
	}

	ProcedureExecuteResult = EProcedureExecuteResult::None;

	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			// Iter->ServerOnPrepare();
			if (bMergeSubProcedure)
			{
				Iter->ServerOnEnter(InLastProcedure);
			}
		}
	}

	MultiOnEnter(InLastProcedure);

	ProcedureState = EProcedureState::Entered;
	OnRep_ProcedureState();
}

void AProcedureBase::MultiOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	if(LocalProcedureState != EProcedureState::Entered)
	{
		LocalOnEnter(InLastProcedure);
	}
}

void AProcedureBase::LocalOnEnter_Implementation(AProcedureBase* InLastProcedure)
{
	LocalProcedureState = EProcedureState::Entered;
	OnChangeLocalProcedureState(LocalProcedureState);

	LocalProcedureExecuteResult = EProcedureExecuteResult::None;
	LocalResetGuide();

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		case EProcedureGuideType::TimerLoop:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &AProcedureBase::LocalGuide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}

	WH_LOG(WHProcedure, Log, TEXT("进入流程: %s"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Cyan, FString::Printf(TEXT("进入流程: %s"), *ProcedureDisplayName.ToString()));

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_EnterProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});
}

void AProcedureBase::ServerOnRefresh_Implementation(float DeltaSeconds)
{
	if (HasSubProcedure(false))
	{
		if(!IsAllSubCompleted())
		{
			const int32 Index = CurrentSubProcedureIndex + 1;
			if(SubProcedures.IsValidIndex(Index))
			{
				AProcedureBase* SubProcedure = SubProcedures[Index];
				if(SubProcedure)
				{
					switch (SubProcedure->GetProcedureExecuteType())
					{
						case EProcedureExecuteType::None:
						{
							SubProcedure->ServerComplete(EProcedureExecuteResult::Skipped);
							CurrentSubProcedureIndex++;
							break;
						}
						case EProcedureExecuteType::Server:
						{
							if(SubProcedure->CheckProcedureCondition(GetCurrentSubProcedure()))
							{
								SubProcedure->ServerPrepare();
							}
							else
							{
								SubProcedure->ServerComplete(EProcedureExecuteResult::Skipped);
								CurrentSubProcedureIndex++;
							}
							break;
						}
						case EProcedureExecuteType::Local:
						{
							SubProcedure->MultiPrepare();
							break;
						}
						default: break;
					}
				}
			}
		}
		else
		{
			ServerComplete(IsAllSubExecuteSucceed() ? EProcedureExecuteResult::Succeed : EProcedureExecuteResult::Failed);
		}
	}
	else if(HasProcedureTask())
	{
		if(!IsAllTaskCompleted())
		{
			const int32 Index = CurrentProcedureTaskIndex + 1;
			if(ProcedureTaskItems.IsValidIndex(Index))
			{
				FProcedureTaskItem& TaskItem = ProcedureTaskItems[Index];
				if(TaskItem.IsValid())
				{
					switch(TaskItem.TaskState)
					{
						case EProcedureTaskState::None:
						{
							TaskItem.Prepare();
							break;
						}
						case EProcedureTaskState::Preparing:
						{
							TaskItem.TryExecute(GetWorld()->GetDeltaSeconds());
							break;
						}
						case EProcedureTaskState::Executing:
						{
							TaskItem.TryComplete(GetWorld()->GetDeltaSeconds());
							break;
						}
						case EProcedureTaskState::Completed:
						{
							CurrentProcedureTaskIndex++;
							break;
						}
					}
				}
			}
		}
		else
		{
			ServerComplete(EProcedureExecuteResult::Succeed);
		}
	}

	MultiOnRefresh(DeltaSeconds);
}

void AProcedureBase::MultiOnRefresh_Implementation(float DeltaSeconds)
{
	LocalOnRefresh(DeltaSeconds);
}

void AProcedureBase::LocalOnRefresh_Implementation(float DeltaSeconds)
{
	if(GetProcedureExecuteType() == EProcedureExecuteType::Local)
	{
		if(HasSubProcedure(false))
		{
			if(!IsAllLocalSubCompleted())
			{
				const int32 Index = CurrentLocalSubProcedureIndex + 1;
				if(SubProcedures.IsValidIndex(Index))
				{
					AProcedureBase* SubProcedure = SubProcedures[Index];
					if(SubProcedure)
					{
						switch (SubProcedure->GetProcedureExecuteType())
						{
							case EProcedureExecuteType::None:
							case EProcedureExecuteType::Server:
							{
								SubProcedure->LocalComplete(EProcedureExecuteResult::Skipped);
								break;
							}
							case EProcedureExecuteType::Local:
							{
								if(SubProcedure->CheckProcedureCondition(GetCurrentLocalSubProcedure()))
								{
									SubProcedure->LocalPrepare();
								}
								else
								{
									SubProcedure->LocalComplete(EProcedureExecuteResult::Skipped);
								}
								break;
							}
							default: break;
						}
					}
				}
			}
			else
			{
				LocalComplete();
			}
		}
	}
}


void AProcedureBase::LocalOnGuide_Implementation(const FName& InListenerTaskName)
{
	
}

void AProcedureBase::LocalOnStopGuide_Implementation(const FName& InListenerTaskName)
{
	
}

void AProcedureBase::ServerOnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	MultiOnComplete(InProcedureExecuteResult);
	
	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerOnce:
		{
			GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);
			break;
		}
		case EProcedureGuideType::TimerLoop:
		{
			GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);
			break;
		}
		default: break;
	}

	ProcedureState = EProcedureState::Completed;
	OnRep_ProcedureState();
}

void AProcedureBase::MultiOnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	if(LocalProcedureState != EProcedureState::Completed)
	{
		LocalOnComplete(InProcedureExecuteResult);
	}
}

void AProcedureBase::LocalOnComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	LocalProcedureState = EProcedureState::Completed;
	OnChangeLocalProcedureState(LocalProcedureState);
}

void AProcedureBase::ServerOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);

	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			if (bMergeSubProcedure || Iter->ProcedureState == EProcedureState::Entered)
			{
				Iter->ServerOnLeave(InNextProcedure);
			}
			Iter->ServerOnClear();
		}
	}

	MultiOnLeave(InNextProcedure);

	ProcedureState = EProcedureState::Leaved;
	OnRep_ProcedureState();
}

void AProcedureBase::MultiOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	if(LocalProcedureState != EProcedureState::Leaved)
	{
		LocalOnLeave(InNextProcedure);
	}
}

void AProcedureBase::LocalOnLeave_Implementation(AProcedureBase* InNextProcedure)
{
	LocalProcedureState = EProcedureState::Leaved;
	OnChangeLocalProcedureState(LocalProcedureState);

	LocalStopGuide();

	WH_LOG(WHProcedure, Log, TEXT("%s流程: %s"), LocalProcedureExecuteResult != EProcedureExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *ProcedureDisplayName.ToString());
	GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Orange, FString::Printf(TEXT("%s流程: %s"), LocalProcedureExecuteResult != EProcedureExecuteResult::Skipped ? TEXT("离开") : TEXT("跳过"), *ProcedureDisplayName.ToString()));

	UEventModuleBPLibrary::BroadcastEvent(UEventHandle_LeaveProcedure::StaticClass(), EEventNetType::Single, this, TArray<FParameter>{FParameter::MakeObject(this)});
}

void AProcedureBase::ServerOnClear_Implementation()
{
	MultiOnClear();
}

void AProcedureBase::MultiOnClear_Implementation()
{
	LocalOnClear();
}

void AProcedureBase::LocalOnClear_Implementation()
{
	GetWorld()->GetTimerManager().ClearTimer(AutoEnterTimerHandle); 
	GetWorld()->GetTimerManager().ClearTimer(AutoLeaveTimerHandle);
}

void AProcedureBase::ServerOnDestroy_Implementation()
{
	MultiOnDestroy();
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			Iter->ServerOnDestroy();
			Iter->Destroy();
		}
	}
}

void AProcedureBase::MultiOnDestroy_Implementation()
{
	SubProcedures.Empty();
	
	LocalOnDestroy();
}

void AProcedureBase::LocalOnDestroy_Implementation()
{
	
}

void AProcedureBase::OnRep_ProcedureState()
{
	if(LocalProcedureState != ProcedureState)
	{
		switch (ProcedureState)
		{
			case EProcedureState::Initialized:
			{
				LocalOnInitialize();
				break;
			}
			case EProcedureState::Prepared:
			{
				LocalOnPrepare();
				break;
			}
			case EProcedureState::Entered:
			{
				LocalOnEnter(nullptr);
				break;
			}
			case EProcedureState::Completed:
			{
				LocalOnComplete(LocalProcedureExecuteResult);
				break;
			}
			case EProcedureState::Leaved:
			{
				LocalOnLeave(nullptr);
				break;
			}
			default: break;
		}
	}
	OnChangeProcedureState(ProcedureState);
}

void AProcedureBase::OnChangeProcedureState_Implementation(EProcedureState InProcedureState)
{
	
}

void AProcedureBase::OnChangeLocalProcedureState_Implementation(EProcedureState InProcedureState)
{
	
}

EProcedureExecuteType AProcedureBase::GetProcedureExecuteType() const
{
	if(ProcedureExecuteType == EProcedureExecuteType::Inherit)
	{
		return ParentProcedure ? ParentProcedure->GetProcedureExecuteType() : EProcedureExecuteType::None;
	}
	return ProcedureExecuteType;
}

bool AProcedureBase::CheckProcedureCondition(AProcedureBase* InProcedure) const
{
	return ProcedureExecuteCondition == EProcedureExecuteResult::None || !InProcedure || InProcedure->ProcedureExecuteResult == ProcedureExecuteCondition;
}

bool AProcedureBase::CheckLocalProcedureCondition(AProcedureBase* InProcedure) const
{
	return ProcedureExecuteCondition == EProcedureExecuteResult::None || !InProcedure || InProcedure->LocalProcedureExecuteResult == ProcedureExecuteCondition;
}

void AProcedureBase::ServerPrepare()
{
	if(ProcedureState == EProcedureState::Prepared) return;
	
	ServerOnPrepare();

	if(bAutoEnterProcedure && ProcedureState != EProcedureState::Entered)
	{
		if(AutoEnterProcedureTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoEnterTimerHandle, FTimerDelegate::CreateStatic<AProcedureBase*>(&UProcedureModuleBPLibrary::ServerEnterProcedure, this), AutoEnterProcedureTime, false);
		}
		else
		{
			UProcedureModuleBPLibrary::ServerEnterProcedure(this);
		}
	}
}

void AProcedureBase::MultiPrepare_Implementation()
{
	if(!HasAuthority())
	{
		LocalPrepare();
	}
}

void AProcedureBase::LocalPrepare()
{
	if(LocalProcedureState == EProcedureState::Prepared) return;

	LocalOnPrepare();

	if(bAutoEnterProcedure && LocalProcedureState != EProcedureState::Entered)
	{
		if(AutoEnterProcedureTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoEnterTimerHandle, FTimerDelegate::CreateStatic<AProcedureBase*>(&UProcedureModuleBPLibrary::LocalEnterProcedure, this), AutoEnterProcedureTime, false);
		}
		else
		{
			UProcedureModuleBPLibrary::ServerEnterProcedure(this);
		}
	}
}

void AProcedureBase::LocalGuide()
{
	if(LocalProcedureState != EProcedureState::Entered) return;

	LocalOnGuide(NAME_None);

	switch(ProcedureGuideType)
	{
		case EProcedureGuideType::TimerLoop:
		{
			GetWorld()->GetTimerManager().SetTimer(StartGuideTimerHandle, this, &AProcedureBase::LocalGuide, ProcedureGuideIntervalTime, false);
			break;
		}
		default: break;
	}
}

void AProcedureBase::LocalResetGuide()
{
	GetWorld()->GetTimerManager().SetTimer(ResetGuideTimerHandle, this, &AProcedureBase::LocalResetGuideImpl, 1.f, false);
}

void AProcedureBase::LocalResetGuideImpl()
{
	
}

void AProcedureBase::LocalStopGuide()
{
	GetWorld()->GetTimerManager().ClearTimer(StartGuideTimerHandle);
}

void AProcedureBase::ServerComplete(EProcedureExecuteResult InProcedureExecuteResult)
{
	if(ProcedureState == EProcedureState::Completed) return;

	ProcedureExecuteResult = InProcedureExecuteResult;

	ServerOnComplete(InProcedureExecuteResult);

	for(auto Iter : SubProcedures)
	{
		if(Iter)
		{
			Iter->ServerComplete(InProcedureExecuteResult);
		}
	}
	
	if(bAutoLeaveProcedure)
	{
		if(InProcedureExecuteResult != EProcedureExecuteResult::Skipped && AutoLeaveProcedureTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoLeaveTimerHandle, FTimerDelegate::CreateStatic<AProcedureBase*>(&UProcedureModuleBPLibrary::ServerLeaveProcedure, this), AutoLeaveProcedureTime, false);
		}
		else
		{
			UProcedureModuleBPLibrary::ServerLeaveProcedure(this);
		}
	}
}

void AProcedureBase::MultiComplete_Implementation(EProcedureExecuteResult InProcedureExecuteResult)
{
	if(!HasAuthority())
	{
		LocalComplete(InProcedureExecuteResult);
	}
}

void AProcedureBase::LocalComplete(EProcedureExecuteResult InProcedureExecuteResult)
{
	if(LocalProcedureState == EProcedureState::Completed) return;

	LocalProcedureExecuteResult = InProcedureExecuteResult;

	LocalOnComplete(InProcedureExecuteResult);

	for(auto Iter : SubProcedures)
	{
		if(Iter)
		{
			Iter->LocalOnComplete(InProcedureExecuteResult);
		}
	}

	if(bAutoLeaveProcedure)
	{
		if(InProcedureExecuteResult != EProcedureExecuteResult::Skipped && AutoLeaveProcedureTime > 0.f)
		{
			GetWorld()->GetTimerManager().SetTimer(AutoLeaveTimerHandle, FTimerDelegate::CreateStatic<AProcedureBase*>(&UProcedureModuleBPLibrary::LocalLeaveProcedure, this), AutoLeaveProcedureTime, false);
		}
		else
		{
			UProcedureModuleBPLibrary::LocalLeaveProcedure(this);
		}
	}
}

bool AProcedureBase::HasSubProcedure(bool bIgnoreMerge) const
{
	return SubProcedures.Num() > 0 && (bIgnoreMerge || !bMergeSubProcedure);
}

AProcedureBase* AProcedureBase::GetCurrentSubProcedure() const
{
	if (SubProcedures.IsValidIndex(CurrentSubProcedureIndex))
	{
		return SubProcedures[CurrentSubProcedureIndex];
	}
	return nullptr;
}

AProcedureBase* AProcedureBase::GetCurrentLocalSubProcedure() const
{
	if (SubProcedures.IsValidIndex(CurrentLocalSubProcedureIndex))
	{
		return SubProcedures[CurrentLocalSubProcedureIndex];
	}
	return nullptr;
}

bool AProcedureBase::IsAllSubCompleted() const
{
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			switch(Iter->ProcedureState)
			{
				case EProcedureState::Completed:
				case EProcedureState::Leaved:
				{
					break;
				}
				default:
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool AProcedureBase::IsAllLocalSubCompleted() const
{
	for (auto Iter : SubProcedures)
	{
		if (Iter)
		{
			switch(Iter->LocalProcedureState)
			{
				case EProcedureState::Completed:
				case EProcedureState::Leaved:
				{
					break;
				}
				default:
				{
					return false;
				}
			}
		}
	}
	return true;
}

bool AProcedureBase::IsAllSubExecuteSucceed() const
{
	for (auto Iter : SubProcedures)
	{
		if (Iter && Iter->ProcedureExecuteResult == EProcedureExecuteResult::Failed)
		{
			return false;
		}
	}
	return true;
}

bool AProcedureBase::IsAllLocalSubExecuteSucceed() const
{
	for (auto Iter : SubProcedures)
	{
		if (Iter && Iter->LocalProcedureExecuteResult == EProcedureExecuteResult::Failed)
		{
			return false;
		}
	}
	return true;
}

FName AProcedureBase::GetListenerTaskName(const FString& InTaskName) const
{
	return *FString::Printf(TEXT("%s_%s"), *ProcedureName.ToString(), *InTaskName);
}

bool AProcedureBase::HasProcedureTask() const
{
	return ProcedureTaskItems.Num() > 0;
}

bool AProcedureBase::IsAllTaskCompleted() const
{
	for (auto Iter : ProcedureTaskItems)
	{
		if (Iter.TaskState != EProcedureTaskState::Completed)
		{
			return false;
		}
	}
	return true;
}

FProcedureTaskItem& AProcedureBase::AddProcedureTask(const FName InTaskName, float InDurationTime, float InStartDelayTime)
{
	ProcedureTaskItems.Add(FProcedureTaskItem(InTaskName, InDurationTime, InStartDelayTime));
	return ProcedureTaskItems[ProcedureTaskItems.Num() - 1];
}

void AProcedureBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AProcedureBase, ProcedureName);
	DOREPLIFETIME(AProcedureBase, ProcedureDisplayName);
	DOREPLIFETIME(AProcedureBase, ProcedureDescription);
	DOREPLIFETIME(AProcedureBase, ProcedureIndex);
	DOREPLIFETIME(AProcedureBase, ProcedureState);
	DOREPLIFETIME(AProcedureBase, ProcedureExecuteType);
	DOREPLIFETIME(AProcedureBase, ProcedureExecuteCondition);
	DOREPLIFETIME(AProcedureBase, ProcedureExecuteResult);
	DOREPLIFETIME(AProcedureBase, bAutoEnterProcedure);
	DOREPLIFETIME(AProcedureBase, AutoEnterProcedureTime);
	DOREPLIFETIME(AProcedureBase, bAutoLeaveProcedure);
	DOREPLIFETIME(AProcedureBase, AutoLeaveProcedureTime);
	DOREPLIFETIME(AProcedureBase, ParentProcedure);
	DOREPLIFETIME(AProcedureBase, bMergeSubProcedure);
	DOREPLIFETIME(AProcedureBase, CurrentSubProcedureIndex);
	DOREPLIFETIME(AProcedureBase, SubProcedures);
	DOREPLIFETIME(AProcedureBase, CurrentProcedureTaskIndex);
}
