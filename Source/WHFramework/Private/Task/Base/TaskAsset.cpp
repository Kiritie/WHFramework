// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Base/TaskAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Task/Base/TaskBase.h"
#include "Task/TaskModuleTypes.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UTaskAsset::UTaskAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();
	
	NativeClass = GetClass();

	RootTasks = TArray<UTaskBase*>();
	TaskMap = TMap<FString, UTaskBase*>();
}

void UTaskAsset::Initialize()
{
	Super::Initialize();
	if (!RebuildTaskMap()) return;
	for (UTaskBase* Task : RootTasks) Task->OnInitialize();
}

UWorld* UTaskAsset::GetWorld() const
{
	return SourceObject && GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

bool UTaskAsset::ValidateTasks(TArray<FText>& OutErrors) const
{
	TSet<const UTaskBase*> Visited;
	TSet<FString> GUIDs;
	TFunction<void(const TArray<UTaskBase*>&)> Visit = [&](const TArray<UTaskBase*>& Tasks)
	{
		for (const UTaskBase* Task : Tasks)
		{
			if (!Task || Visited.Contains(Task))
			{
				OutErrors.Add(FText::FromString(TEXT("Task hierarchy contains a null, shared or cyclic node.")));
				continue;
			}
			Visited.Add(Task);
			if (Task->GetTaskAsset() != this || Task->TaskGUID.IsEmpty() || GUIDs.Contains(Task->TaskGUID))
			{
				OutErrors.Add(FText::Format(NSLOCTEXT("Task", "InvalidIdentity", "Invalid owner or duplicate/missing GUID: {0}"), Task->TaskDisplayName));
			}
			GUIDs.Add(Task->TaskGUID);
			TSet<FName> ObjectiveIDs;
			for (const FTaskObjective& Objective : Task->Objectives)
			{
				if (Objective.ObjectiveID.IsNone() || ObjectiveIDs.Contains(Objective.ObjectiveID) || !Objective.EventTag.IsValid() || Objective.RequiredCount < 1 ||
					(Objective.RequiredCountRange != FIntPoint::ZeroValue && (Objective.RequiredCountRange.X < 1 || Objective.RequiredCountRange.Y < Objective.RequiredCountRange.X)))
				{
					OutErrors.Add(FText::Format(NSLOCTEXT("Task", "InvalidObjective", "Invalid objective in task: {0}"), Task->TaskDisplayName));
				}
				ObjectiveIDs.Add(Objective.ObjectiveID);
			}
			Visit(Task->SubTasks);
		}
	};
	Visit(RootTasks);
	const UObject* Source = SourceObject ? SourceObject : this;
	for (const UTaskBase* Task : Visited)
	{
		for (const FTaskReference& Reference : Task->Prerequisites)
		{
			const bool bLocal = Reference.Asset.ToSoftObjectPath() == FSoftObjectPath(Source);
			if (Reference.Asset.IsNull() || Reference.TaskGUID.IsEmpty() ||
				(bLocal && (!GUIDs.Contains(Reference.TaskGUID) || Reference.TaskGUID == Task->TaskGUID)))
			{
				OutErrors.Add(FText::Format(NSLOCTEXT("Task", "InvalidPrerequisite", "Invalid prerequisite in task: {0}"), Task->TaskDisplayName));
			}
		}
	}
	return OutErrors.IsEmpty();
}

bool UTaskAsset::RebuildTaskMap(bool bValidateDefinition)
{
	TaskMap.Reset();
	TArray<FText> Errors;
	if (bValidateDefinition && !ValidateTasks(Errors))
	{
		for (const FText& Error : Errors) UE_LOG(LogTemp, Warning, TEXT("%s: %s"), *GetPathName(), *Error.ToString());
		return false;
	}
	TSet<UTaskBase*> Visited;
	bool bValidHierarchy = true;
	TFunction<void(const TArray<UTaskBase*>&, UTaskBase*, UTaskBase*, int32)> Visit =
		[&](const TArray<UTaskBase*>& Tasks, UTaskBase* Parent, UTaskBase* Root, int32 Depth)
	{
		for (int32 Index = 0; Index < Tasks.Num(); ++Index)
		{
			UTaskBase* Task = Tasks[Index];
			if (!Task || Visited.Contains(Task) || Task->GetTaskAsset() != this || Task->TaskGUID.IsEmpty() || TaskMap.Contains(Task->TaskGUID))
			{
				bValidHierarchy = false;
				continue;
			}
			Visited.Add(Task);
			Task->TaskIndex = Index;
			Task->TaskHierarchy = Depth;
			Task->ParentTask = Parent;
			Task->RootTask = Root;
			TaskMap.Add(Task->TaskGUID, Task);
			Visit(Task->SubTasks, Task, Root ? Root : Task, Depth + 1);
		}
	};
	Visit(RootTasks, nullptr, nullptr, 0);
	return bValidHierarchy;
}

bool UTaskAsset::IsAllTaskCompleted() const
{
	for (auto Iter : RootTasks)
	{
		if(Iter && !Iter->IsCompleted(true))
		{
			return false;
		}
	}
	return true;
}

bool UTaskAsset::IsAllTaskLeaved() const
{
	for (auto Iter : RootTasks)
	{
		if(Iter && !Iter->IsLeaved(true))
		{
			return false;
		}
	}
	return true;
}

FTaskReference UTaskAsset::MakeTaskReference(const FString& InTaskGUID) const
{
	FTaskReference Reference;
	Reference.Asset = Cast<UTaskAsset>(SourceObject ? SourceObject : const_cast<UTaskAsset*>(this));
	Reference.InstanceID = InstanceID;
	Reference.TaskGUID = InTaskGUID;
	return Reference;
}

#if WITH_EDITOR
EDataValidationResult UTaskAsset::IsDataValid(FDataValidationContext& Context) const
{
	TArray<FText> Errors;
	const bool bValid = ValidateTasks(Errors);
	for (const FText& Error : Errors) Context.AddError(Error);
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}

bool UTaskAsset::CanAddTask(TSubclassOf<UTaskBase> InTaskClass)
{
	return true;
}

void UTaskAsset::ClearAllTask()
{
	Modify();
	for(auto Iter : RootTasks)
	{
		if(Iter)
		{
			Iter->OnUnGenerate();

		}
	}
	
	RootTasks.Empty();

	TaskMap.Empty();

	Modify();
}
#endif
