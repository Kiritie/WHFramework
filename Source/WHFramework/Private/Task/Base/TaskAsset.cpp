// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Base/TaskAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Task/TaskModuleTypes.h"
#include "Misc/SecureHash.h"
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

bool UTaskAsset::InitializeRuntimeTasks()
{
	if(!RebuildTaskMap(false)) return false;
	Super::Initialize();
	for(UTaskBase* Task : RootTasks)
	{
		if(Task) Task->OnInitialize();
	}
	return true;
}

UWorld* UTaskAsset::GetWorld() const
{
	return SourceObject && GetOuter() ? GetOuter()->GetWorld() : nullptr;
}

namespace TaskAssetReferences
{
	FString MakeRuntimeTaskGUID(const FString& ReferenceTaskGUID, const FString& SourceTaskGUID)
	{
		return FMD5::HashAnsiString(*(ReferenceTaskGUID + TEXT("/") + SourceTaskGUID));
	}

	void AddError(TArray<FText>& OutErrors, const UTaskAsset* Asset, const FString& Message)
	{
		OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s: %s"), Asset ? *Asset->GetPathName() : TEXT("None"), *Message)));
	}

	void VisitTask(const UTaskBase* Task, TFunctionRef<void(const UTaskBase*)> Function)
	{
		Function(Task);
		for(const UTaskBase* Child : Task->SubTasks)
		{
			if(Child) VisitTask(Child, Function);
		}
	}

	void ValidateDefinition(const UTaskAsset* Asset, TArray<FText>& OutErrors, TSet<const UTaskAsset*>& ActiveAssets, TSet<const UTaskAsset*>& ValidatedAssets)
	{
		if(!Asset) return;
		if(ActiveAssets.Contains(Asset))
		{
			AddError(OutErrors, Asset, TEXT("Task asset references contain a cycle."));
			return;
		}
		if(ValidatedAssets.Contains(Asset)) return;
		ActiveAssets.Add(Asset);

		TSet<const UTaskBase*> Visited;
		TSet<FString> GUIDs;
		TArray<const UTaskBase*> Tasks;
		TFunction<void(const TArray<UTaskBase*>&)> Visit = [&](const TArray<UTaskBase*>& Nodes)
		{
			for(const UTaskBase* Task : Nodes)
			{
				if(!Task || Visited.Contains(Task))
				{
					AddError(OutErrors, Asset, TEXT("Task hierarchy contains a null, shared or cyclic node."));
					continue;
				}
				Visited.Add(Task);
				Tasks.Add(Task);
				if(Task->GetTaskAsset() != Asset || Task->TaskGUID.IsEmpty() || GUIDs.Contains(Task->TaskGUID))
				{
					AddError(OutErrors, Asset, FString::Printf(TEXT("Invalid owner or duplicate/missing GUID: %s"), *Task->TaskDisplayName.ToString()));
				}
				GUIDs.Add(Task->TaskGUID);
				TSet<FName> ObjectiveIDs;
				for(const FTaskObjective& Objective : Task->Objectives)
				{
					if(Objective.ObjectiveID.IsNone() || ObjectiveIDs.Contains(Objective.ObjectiveID) || !Objective.EventTag.IsValid() || Objective.RequiredCount < 1 ||
						(Objective.RequiredCountRange != FIntPoint::ZeroValue && (Objective.RequiredCountRange.X < 1 || Objective.RequiredCountRange.Y < Objective.RequiredCountRange.X)))
					{
						AddError(OutErrors, Asset, FString::Printf(TEXT("Invalid objective in task: %s"), *Task->TaskDisplayName.ToString()));
					}
					ObjectiveIDs.Add(Objective.ObjectiveID);
				}
				Visit(Task->SubTasks);
			}
		};
		Visit(Asset->RootTasks);

		const FSoftObjectPath AssetPath(Asset);
		for(const UTaskBase* Task : Tasks)
		{
			for(const FTaskReference& Reference : Task->Prerequisites)
			{
				const bool bLocal = Reference.Asset.ToSoftObjectPath() == AssetPath;
				if(Reference.Asset.IsNull() || Reference.TaskGUID.IsEmpty() || (bLocal && (!GUIDs.Contains(Reference.TaskGUID) || Reference.TaskGUID == Task->TaskGUID)))
				{
					AddError(OutErrors, Asset, FString::Printf(TEXT("Invalid prerequisite in task: %s"), *Task->TaskDisplayName.ToString()));
				}
			}

			if(const UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
			{
				if(!ReferenceTask->SubTasks.IsEmpty())
				{
					AddError(OutErrors, Asset, FString::Printf(TEXT("Task asset reference must not have authored child tasks: %s"), *Task->TaskDisplayName.ToString()));
				}
				UTaskAsset* ReferencedAsset = ReferenceTask->ReferencedAsset.LoadSynchronous();
				if(!ReferencedAsset)
				{
					AddError(OutErrors, Asset, FString::Printf(TEXT("Task asset reference is empty or missing: %s"), *Task->TaskDisplayName.ToString()));
				}
				else if(ReferencedAsset->RootTasks.IsEmpty())
				{
					AddError(OutErrors, Asset, FString::Printf(TEXT("Referenced task asset has no root tasks: %s"), *ReferencedAsset->GetPathName()));
				}
				else
				{
					ValidateDefinition(ReferencedAsset, OutErrors, ActiveAssets, ValidatedAssets);
				}
			}
		}

		ActiveAssets.Remove(Asset);
		ValidatedAssets.Add(Asset);
	}
}

bool UTaskAsset::ExpandTaskAssetReferences(TArray<FText>& OutErrors)
{
	const UTaskAsset* RuntimeSource = Cast<UTaskAsset>(SourceObject);
	if(!RuntimeSource)
	{
		TaskAssetReferences::AddError(OutErrors, this, TEXT("Only runtime task asset duplicates can expand task references."));
		return false;
	}

	TSet<const UTaskAsset*> ActiveAssets;
	ActiveAssets.Add(RuntimeSource);
	TFunction<bool(UTaskBase*, const UTaskAsset*)> ExpandTask = [&](UTaskBase* Task, const UTaskAsset* DefinitionAsset) -> bool
	{
		if(!Task) return false;
		if(UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
		{
			if(ReferenceTask->bRuntimeExpanded) return true;
			if(!ReferenceTask->SubTasks.IsEmpty())
			{
				TaskAssetReferences::AddError(OutErrors, DefinitionAsset, FString::Printf(TEXT("Task asset reference must not have authored child tasks: %s"), *Task->TaskDisplayName.ToString()));
				return false;
			}
			UTaskAsset* ReferencedAsset = ReferenceTask->ReferencedAsset.Get();
			if(!ReferencedAsset || ReferencedAsset->RootTasks.IsEmpty())
			{
				TaskAssetReferences::AddError(OutErrors, DefinitionAsset, FString::Printf(TEXT("Task asset reference is empty, missing, or has no roots: %s"), *Task->TaskDisplayName.ToString()));
				return false;
			}
			if(ActiveAssets.Contains(ReferencedAsset))
			{
				TaskAssetReferences::AddError(OutErrors, ReferencedAsset, TEXT("Task asset references contain a cycle."));
				return false;
			}

			ActiveAssets.Add(ReferencedAsset);
			UTaskAsset* DuplicatedAsset = DuplicateObject<UTaskAsset>(
				ReferencedAsset,
				this,
				MakeUniqueObjectName(this, UTaskAsset::StaticClass(), TEXT("ReferencedTaskAsset")));
			if(!DuplicatedAsset)
			{
				TaskAssetReferences::AddError(OutErrors, ReferencedAsset, TEXT("Failed to duplicate referenced task asset."));
				ActiveAssets.Remove(ReferencedAsset);
				return false;
			}

			TArray<UTaskBase*> DuplicatedRoots = DuplicatedAsset->RootTasks;
			TArray<UTaskBase*> DuplicatedTasks;
			TMap<FString, FString> RemappedGUIDs;
			for(UTaskBase* DuplicatedRoot : DuplicatedRoots)
			{
				if(!DuplicatedRoot) continue;
				TaskAssetReferences::VisitTask(DuplicatedRoot, [&](const UTaskBase* Item)
				{
					DuplicatedTasks.Add(const_cast<UTaskBase*>(Item));
				});
			}
			if(DuplicatedRoots.Num() != ReferencedAsset->RootTasks.Num())
			{
				TaskAssetReferences::AddError(OutErrors, ReferencedAsset, TEXT("Failed to duplicate all referenced root tasks."));
				ActiveAssets.Remove(ReferencedAsset);
				return false;
			}
			for(UTaskBase* DuplicatedTask : DuplicatedTasks)
			{
				const FName UniqueName = MakeUniqueObjectName(this, DuplicatedTask->GetClass(), DuplicatedTask->GetFName());
				if(!DuplicatedTask->Rename(*UniqueName.ToString(), this, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional))
				{
					TaskAssetReferences::AddError(OutErrors, ReferencedAsset, TEXT("Failed to move duplicated tasks into the runtime task asset."));
					ActiveAssets.Remove(ReferencedAsset);
					return false;
				}
			}
			DuplicatedAsset->RootTasks.Reset();
			DuplicatedAsset->TaskMap.Reset();

			for(UTaskBase* DuplicatedTask : DuplicatedTasks)
			{
				const FString SourceGUID = DuplicatedTask->TaskGUID;
				if(SourceGUID.IsEmpty() || RemappedGUIDs.Contains(SourceGUID))
				{
					TaskAssetReferences::AddError(OutErrors, ReferencedAsset, TEXT("Referenced task identities are missing or duplicated."));
					ActiveAssets.Remove(ReferencedAsset);
					return false;
				}
				RemappedGUIDs.Add(SourceGUID, TaskAssetReferences::MakeRuntimeTaskGUID(ReferenceTask->TaskGUID, SourceGUID));
			}

			for(UTaskBase* DuplicatedTask : DuplicatedTasks)
			{
				DuplicatedTask->TaskGUID = RemappedGUIDs.FindChecked(DuplicatedTask->TaskGUID);
				DuplicatedTask->ParentTask = nullptr;
				DuplicatedTask->RootTask = nullptr;
				for(FTaskReference& Prerequisite : DuplicatedTask->Prerequisites)
				{
					const FSoftObjectPath PrerequisiteAssetPath = Prerequisite.Asset.ToSoftObjectPath();
					if(PrerequisiteAssetPath == FSoftObjectPath(ReferencedAsset) || PrerequisiteAssetPath == FSoftObjectPath(DuplicatedAsset))
					{
						if(const FString* RemappedGUID = RemappedGUIDs.Find(Prerequisite.TaskGUID))
						{
							Prerequisite.Asset = const_cast<UTaskAsset*>(RuntimeSource);
							Prerequisite.InstanceID.Invalidate();
							Prerequisite.TaskGUID = *RemappedGUID;
						}
					}
				}
			}

			ReferenceTask->SubTasks = MoveTemp(DuplicatedRoots);
			ReferenceTask->bRuntimeExpanded = true;
			bool bExpanded = true;
			for(UTaskBase* DuplicatedRoot : ReferenceTask->SubTasks)
			{
				TFunction<void(UTaskBase*)> ExpandTree = [&](UTaskBase* Node)
				{
					if(!bExpanded || !Node) return;
					if(Cast<UTaskAssetReferenceTask>(Node)) bExpanded = ExpandTask(Node, ReferencedAsset);
					else for(UTaskBase* Child : Node->SubTasks) ExpandTree(Child);
				};
				ExpandTree(DuplicatedRoot);
			}
			ActiveAssets.Remove(ReferencedAsset);
			return bExpanded;
		}

		for(UTaskBase* Child : Task->SubTasks)
		{
			if(!ExpandTask(Child, DefinitionAsset)) return false;
		}
		return true;
	};

	for(UTaskBase* RootTask : RootTasks)
	{
		if(!ExpandTask(RootTask, RuntimeSource)) return false;
	}
	return OutErrors.IsEmpty();
}

bool UTaskAsset::ValidateTasks(TArray<FText>& OutErrors) const
{
	if(!SourceObject)
	{
		TSet<const UTaskAsset*> ActiveAssets;
		TSet<const UTaskAsset*> ValidatedAssets;
		TaskAssetReferences::ValidateDefinition(this, OutErrors, ActiveAssets, ValidatedAssets);
		return OutErrors.IsEmpty();
	}

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
			if(const UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task); ReferenceTask && !ReferenceTask->IsRuntimeExpanded())
			{
				OutErrors.Add(FText::Format(NSLOCTEXT("Task", "UnexpandedReference", "Runtime task asset contains an unexpanded reference: {0}"), Task->TaskDisplayName));
			}
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
