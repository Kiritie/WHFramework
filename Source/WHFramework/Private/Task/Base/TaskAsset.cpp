// Copyright Epic Games, Inc. All Rights Reserved.

#include "Task/Base/TaskAsset.h"
#include "AssetRegistry/AssetData.h"
#include "Task/Base/TaskBase.h"

UTaskAsset::UTaskAsset(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DisplayName = FText::GetEmpty();
	
	NativeClass = GetClass();

	RootTasks = TArray<UTaskBase*>();
	TaskMap = TMap<FString, UTaskBase*>();

	FirstTask = nullptr;
}

void UTaskAsset::Initialize(UAssetBase* InSource)
{
	Super::Initialize(InSource);

	for(const auto Iter : RootTasks)
	{
		if(!Iter) continue;

		Iter->OnInitialize();

		if(!FirstTask)
		{
			FirstTask = Iter;
		}
	}
}

#if WITH_EDITOR
void UTaskAsset::GenerateTaskListItem(TArray<TSharedPtr<FTaskListItem>>& OutTaskListItems)
{
	OutTaskListItems = TArray<TSharedPtr<FTaskListItem>>();
	for (int32 i = 0; i < RootTasks.Num(); i++)
	{
		auto Item = MakeShared<FTaskListItem>();
		RootTasks[i]->GenerateListItem(Item);
		OutTaskListItems.Add(Item);
	}
}

void UTaskAsset::UpdateTaskListItem(TArray<TSharedPtr<FTaskListItem>>& OutTaskListItems)
{
	for (int32 i = 0; i < RootTasks.Num(); i++)
	{
		RootTasks[i]->TaskIndex = i;
		RootTasks[i]->TaskHierarchy = 0;
		RootTasks[i]->UpdateListItem(OutTaskListItems[i]);
	}
}

bool UTaskAsset::CanAddTask(TSubclassOf<UTaskBase> InTaskClass)
{
	return true;
}

void UTaskAsset::ClearAllTask()
{
	for(auto Iter : RootTasks)
	{
		if(Iter)
		{
			Iter->OnUnGenerate();
			Iter->ConditionalBeginDestroy();
		}
	}
	
	RootTasks.Empty();

	TaskMap.Empty();

	Modify();
}
#endif
