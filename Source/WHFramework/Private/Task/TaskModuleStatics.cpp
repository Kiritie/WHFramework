// Fill out your copyright notice in the Description page of Project Settings.


#include "Task/TaskModuleStatics.h"
#include "Task/TaskExecutionTypes.h"
#include "Task/TaskReferenceTypes.h"
#include "Task/TaskObjectiveTypes.h"

#include "Task/TaskModule.h"

TArray<UTaskAsset*> UTaskModuleStatics::GetTaskAssets(UWorld* InWorld, FGuid InPlayerId)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->GetVisibleAssets(InPlayerId);
    return {};
}

UTaskBase* UTaskModuleStatics::GetCurrentTask(UWorld* InWorld, FGuid InPlayerId)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->GetCurrentTask(InPlayerId);
    return {};
}

void UTaskModuleStatics::SetCurrentTask(UWorld* InWorld, FGuid InPlayerId, UTaskBase* InTask)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->SetCurrentTask(InPlayerId, InTask);
}

bool UTaskModuleStatics::IsExistTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->IsExistTaskByGUID(InTaskGUID);
    return {};
}

UTaskBase* UTaskModuleStatics::GetTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->GetTaskByGUID(InTaskGUID);
    return {};
}

UTaskAsset* UTaskModuleStatics::GetTaskAsset(UWorld* InWorld, UTaskAsset* InAsset)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->GetAsset(InAsset);
    return {};
}

UTaskAsset* UTaskModuleStatics::AddTaskAsset(UWorld* InWorld, UTaskAsset* InAsset)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->AddAsset(InAsset);
    return {};
}

UTaskAsset* UTaskModuleStatics::CreateTaskAsset(UWorld* InWorld, UTaskAsset* InAsset, FGuid InAgentID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->CreateAsset(InAsset, InAgentID);
    return {};
}

void UTaskModuleStatics::RemoveTaskAsset(UWorld* InWorld, UTaskAsset* InAsset)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->RemoveAsset(InAsset);
}

void UTaskModuleStatics::RestoreTask(UWorld* InWorld, UTaskBase* InTask)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->RestoreTask(InTask);
}

void UTaskModuleStatics::RestoreTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->RestoreTaskByGUID(InTaskGUID);
}

void UTaskModuleStatics::EnterTask(UWorld* InWorld, UTaskBase* InTask, bool bSetAsCurrent)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->EnterTask(InTask, bSetAsCurrent);
}

void UTaskModuleStatics::EnterTaskByGUID(UWorld* InWorld, const FString& InTaskGUID, bool bSetAsCurrent)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->EnterTaskByGUID(InTaskGUID, bSetAsCurrent);
}

void UTaskModuleStatics::RefreshTask(UWorld* InWorld, UTaskBase* InTask)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->RefreshTask(InTask);
}

void UTaskModuleStatics::RefreshTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->RefreshTaskByGUID(InTaskGUID);
}

void UTaskModuleStatics::GuideTask(UWorld* InWorld, UTaskBase* InTask)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->GuideTask(InTask);
}

void UTaskModuleStatics::GuideTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->GuideTaskByGUID(InTaskGUID);
}

void UTaskModuleStatics::ExecuteTask(UWorld* InWorld, UTaskBase* InTask)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->ExecuteTask(InTask);
}

void UTaskModuleStatics::ExecuteTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->ExecuteTaskByGUID(InTaskGUID);
}

void UTaskModuleStatics::CompleteTask(UWorld* InWorld, UTaskBase* InTask, ETaskExecuteResult InTaskExecuteResult)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->CompleteTask(InTask, InTaskExecuteResult);
}

void UTaskModuleStatics::CompleteTaskByGUID(UWorld* InWorld, const FString& InTaskGUID, ETaskExecuteResult InTaskExecuteResult)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->CompleteTaskByGUID(InTaskGUID, InTaskExecuteResult);
}

void UTaskModuleStatics::LeaveTask(UWorld* InWorld, UTaskBase* InTask)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->LeaveTask(InTask);
}

void UTaskModuleStatics::LeaveTaskByGUID(UWorld* InWorld, const FString& InTaskGUID)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->LeaveTaskByGUID(InTaskGUID);
}

UTaskBase* UTaskModuleStatics::ResolveTask(UWorld* InWorld, const FTaskReference& InReference)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->ResolveTask(InReference);
    return {};
}

bool UTaskModuleStatics::TurnInTask(UWorld* InWorld, UTaskBase* InTask, AActor* InTarget)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) return Module->TurnInTask(InTask, InTarget);
    return {};
}

void UTaskModuleStatics::ReportTaskEvent(UWorld* InWorld, FGameplayTag InEventTag, FGameplayTag InTargetTag, int32 InCount, FPrimaryAssetId InTargetAssetID, FName InTargetName, FGuid InOwnerActorID, ETaskEventScope InScope)
{
    if (UTaskModule* Module = UTaskModule::GetPtr(InWorld)) Module->ReportTaskEvent(InEventTag, InTargetTag, InCount, InTargetAssetID, InTargetName, InOwnerActorID, InScope);
}
