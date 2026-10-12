#include "Task/TaskGraphCompiler.h"

#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Misc/SecureHash.h"

void FTaskGraphCompiler::AddError(const UTaskAsset* Asset, const FString& Message, TArray<FText>& OutErrors)
{
	OutErrors.Add(FText::FromString(FString::Printf(TEXT("%s: %s"), Asset ? *Asset->GetPathName() : TEXT("None"), *Message)));
}

bool FTaskGraphCompiler::Build(const UTaskAsset* Asset, FCompiledTree& OutTree, TArray<FText>& OutErrors)
{
	OutTree = FCompiledTree();
	if(!Asset)
	{
		AddError(Asset, TEXT("任务资产不存在。"), OutErrors);
		return false;
	}
	const int32 InitialErrorCount = OutErrors.Num();
#if WITH_EDITORONLY_DATA
	if(!Asset->StartGUID.IsValid())
	{
		AddError(Asset, TEXT("任务图缺少唯一 Start 身份；必须显式建立作者图。"), OutErrors);
		return false;
	}
	if(!FMath::IsFinite(Asset->StartPosition.X) || !FMath::IsFinite(Asset->StartPosition.Y))
	{
		AddError(Asset, TEXT("Start 节点坐标无效。"), OutErrors);
		return false;
	}
	TSet<UTaskBase*> AuthoredSet;
	TSet<FGuid> AuthoredGUIDs;
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		FGuid ParsedGUID;
		if(Task && !FGuid::Parse(Task->TaskGUID, ParsedGUID))
		{
			FGuid::ParseExact(FMD5::HashAnsiString(*Task->TaskGUID), EGuidFormats::Digits, ParsedGUID);
		}
		if(!Task || Task->GetTaskAsset() != Asset || Task->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
			AuthoredSet.Contains(Task) || Task->TaskGUID.IsEmpty() ||
			!ParsedGUID.IsValid() || ParsedGUID == Asset->StartGUID || AuthoredGUIDs.Contains(ParsedGUID))
		{
			AddError(Asset, TEXT("作者节点包含空值、错误所有者、无效任务类、重复对象或无效/重复 GUID。"), OutErrors);
			continue;
		}
		AuthoredSet.Add(Task);
		AuthoredGUIDs.Add(ParsedGUID);
		if(!FMath::IsFinite(Task->GraphPosition.X) || !FMath::IsFinite(Task->GraphPosition.Y))
		{
			AddError(Asset, FString::Printf(TEXT("节点坐标无效：%s"), *Task->TaskGUID), OutErrors);
		}
	}
	TMap<UTaskBase*, int32> IncomingCounts;
	auto ValidateChildren = [&](const TArray<UTaskBase*>& Children, UTaskBase* Parent)
	{
		TSet<UTaskBase*> UniqueChildren;
		for(UTaskBase* Child : Children)
		{
			if(!Child || !AuthoredSet.Contains(Child) || Child == Parent || UniqueChildren.Contains(Child))
			{
				AddError(Asset, TEXT("连线包含空值、跨资产节点、自连接或重复边。"), OutErrors);
				continue;
			}
			UniqueChildren.Add(Child);
			if(++IncomingCounts.FindOrAdd(Child) > 1)
			{
				AddError(Asset, FString::Printf(TEXT("任务存在多个父节点：%s"), *Child->TaskGUID), OutErrors);
			}
		}
	};
	ValidateChildren(Asset->StartTasks, nullptr);
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		if(!AuthoredSet.Contains(Task))
		{
			continue;
		}
		if(Task->IsA<UTaskAssetReferenceTask>() && !Task->GraphChildren.IsEmpty())
		{
			AddError(Asset, FString::Printf(TEXT("独立任务资产引用节点不能连接作者子节点：%s"), *Task->TaskGUID), OutErrors);
		}
		ValidateChildren(Task->GraphChildren, Task);
	}
	if(OutErrors.Num() != InitialErrorCount)
	{
		return false;
	}
	TMap<UTaskBase*, uint8> Colors;
	TFunction<bool(UTaskBase*)> CheckCycle = [&](UTaskBase* Task)
	{
		uint8& Color = Colors.FindOrAdd(Task);
		if(Color == 1)
		{
			AddError(Asset, FString::Printf(TEXT("任务图存在循环：%s"), *Task->TaskGUID), OutErrors);
			return false;
		}
		if(Color == 2)
		{
			return true;
		}
		Color = 1;
		for(UTaskBase* Child : Task->GraphChildren)
		{
			if(!CheckCycle(Child))
			{
				return false;
			}
		}
		Colors.FindChecked(Task) = 2;
		return true;
	};
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		if(!CheckCycle(Task))
		{
			return false;
		}
	}
	auto SortChildren = [](TArray<UTaskBase*>& Children)
	{
		Children.Sort([](const UTaskBase& Left, const UTaskBase& Right)
		{
			if(Left.GraphPosition.Y != Right.GraphPosition.Y)
			{
				return Left.GraphPosition.Y < Right.GraphPosition.Y;
			}
			if(Left.GraphPosition.X != Right.GraphPosition.X)
			{
				return Left.GraphPosition.X < Right.GraphPosition.X;
			}
			return Left.TaskGUID.Compare(Right.TaskGUID, ESearchCase::CaseSensitive) < 0;
		});
	};
	OutTree.Roots = Asset->StartTasks;
	SortChildren(OutTree.Roots);
	TFunction<void(UTaskBase*)> Collect = [&](UTaskBase* Task)
	{
		OutTree.Tasks.Add(Task->TaskGUID, Task);
		TArray<UTaskBase*> Children = Task->GraphChildren;
		SortChildren(Children);
		OutTree.Children.Add(Task, Children);
		for(UTaskBase* Child : Children)
		{
			Collect(Child);
		}
	};
	for(UTaskBase* Root : OutTree.Roots)
	{
		Collect(Root);
	}
#else
	if(!Asset->bCompiledValid)
	{
		AddError(Asset, TEXT("任务资产未通过编译，拒绝使用旧结构。"), OutErrors);
		return false;
	}
	TSet<UTaskBase*> Visited;
	TFunction<bool(UTaskBase*)> Collect = [&](UTaskBase* Task)
	{
		if(!Task || Task->GetTaskAsset() != Asset || Task->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
			Task->TaskGUID.IsEmpty() || Visited.Contains(Task) || OutTree.Tasks.Contains(Task->TaskGUID))
		{
			AddError(Asset, TEXT("已烘焙任务树包含无效节点或任务类、重复身份、多父或循环。"), OutErrors);
			return false;
		}
		Visited.Add(Task);
		OutTree.Tasks.Add(Task->TaskGUID, Task);
		OutTree.Children.Add(Task, Task->SubTasks);
		for(UTaskBase* Child : Task->SubTasks)
		{
			if(!Collect(Child))
			{
				return false;
			}
		}
		return true;
	};
	OutTree.Roots = Asset->RootTasks;
	for(UTaskBase* Root : OutTree.Roots)
	{
		if(!Collect(Root))
		{
			return false;
		}
	}
#endif
	return OutErrors.Num() == InitialErrorCount;
}

bool FTaskGraphCompiler::ValidateAsset(const UTaskAsset* Asset, TSet<const UTaskAsset*>& ActiveAssets, TSet<const UTaskAsset*>& ValidatedAssets, TArray<FText>& OutErrors)
{
	if(!Asset)
	{
		AddError(Asset, TEXT("任务资产引用无法加载。"), OutErrors);
		return false;
	}
	if(ActiveAssets.Contains(Asset))
	{
		AddError(Asset, TEXT("独立任务资产引用存在循环。"), OutErrors);
		return false;
	}
	if(ValidatedAssets.Contains(Asset))
	{
		return true;
	}
	FCompiledTree Tree;
	if(!Build(Asset, Tree, OutErrors))
	{
		return false;
	}
	const int32 InitialErrorCount = OutErrors.Num();
	ActiveAssets.Add(Asset);
	const UObject* Source = Asset->SourceObject ? Asset->SourceObject : Asset;
	const FSoftObjectPath AssetPath(Source);
	for(const TPair<FString, UTaskBase*>& Pair : Tree.Tasks)
	{
		UTaskBase* Task = Pair.Value;
		TSet<FName> ObjectiveIDs;
		for(const FTaskObjective& Objective : Task->Objectives)
		{
			if(Objective.ObjectiveID.IsNone() || ObjectiveIDs.Contains(Objective.ObjectiveID) || !Objective.EventTag.IsValid() || Objective.RequiredCount < 1 ||
				(Objective.RequiredCountRange != FIntPoint::ZeroValue && (Objective.RequiredCountRange.X < 1 || Objective.RequiredCountRange.Y < Objective.RequiredCountRange.X)))
			{
				AddError(Asset, FString::Printf(TEXT("任务目标定义无效：%s"), *Task->TaskGUID), OutErrors);
			}
			ObjectiveIDs.Add(Objective.ObjectiveID);
		}
		for(const FTaskReference& Prerequisite : Task->Prerequisites)
		{
			const bool bLocal = Prerequisite.Asset.ToSoftObjectPath() == AssetPath || Prerequisite.Asset.ToSoftObjectPath() == FSoftObjectPath(Asset);
			if(Prerequisite.Asset.IsNull() || Prerequisite.TaskGUID.IsEmpty() || (bLocal && (Prerequisite.TaskGUID == Task->TaskGUID || !Tree.Tasks.Contains(Prerequisite.TaskGUID))))
			{
				AddError(Asset, FString::Printf(TEXT("前置任务必须引用有效的 Start 可达节点：%s"), *Task->TaskGUID), OutErrors);
				continue;
			}
			if(!bLocal)
			{
				UTaskAsset* PrerequisiteAsset = Prerequisite.Asset.LoadSynchronous();
				FCompiledTree PrerequisiteTree;
				if(!Build(PrerequisiteAsset, PrerequisiteTree, OutErrors) || !PrerequisiteTree.Tasks.Contains(Prerequisite.TaskGUID))
				{
					AddError(Asset, FString::Printf(TEXT("跨资产前置任务不存在或未连接 Start：%s"), *Task->TaskGUID), OutErrors);
				}
			}
		}
		if(const UTaskAssetReferenceTask* ReferenceTask = Cast<UTaskAssetReferenceTask>(Task))
		{
			if(!Tree.Children.FindChecked(Task).IsEmpty())
			{
				AddError(Asset, FString::Printf(TEXT("任务资产引用不能包含作者子节点：%s"), *Task->TaskGUID), OutErrors);
			}
			UTaskAsset* ReferencedAsset = ReferenceTask->ReferencedAsset.LoadSynchronous();
			if(!ValidateAsset(ReferencedAsset, ActiveAssets, ValidatedAssets, OutErrors))
			{
				continue;
			}
			FCompiledTree ReferencedTree;
			if(!Build(ReferencedAsset, ReferencedTree, OutErrors) || ReferencedTree.Roots.IsEmpty())
			{
				AddError(Asset, FString::Printf(TEXT("被引用任务资产必须至少有一个 Start 可达根任务：%s"), *Task->TaskGUID), OutErrors);
			}
		}
	}
	ActiveAssets.Remove(Asset);
	if(OutErrors.Num() == InitialErrorCount)
	{
		ValidatedAssets.Add(Asset);
		return true;
	}
	return false;
}

bool FTaskGraphCompiler::Validate(const UTaskAsset* Asset, TArray<FText>& OutErrors)
{
	TSet<const UTaskAsset*> ActiveAssets;
	TSet<const UTaskAsset*> ValidatedAssets;
	return ValidateAsset(Asset, ActiveAssets, ValidatedAssets, OutErrors);
}

bool FTaskGraphCompiler::Compile(UTaskAsset* Asset, TArray<FText>& OutErrors)
{
	if(!Asset)
	{
		AddError(Asset, TEXT("任务资产不存在。"), OutErrors);
		return false;
	}
#if WITH_EDITORONLY_DATA
	if(!Asset->StartGUID.IsValid())
	{
		Asset->bCompiledValid = false;
		Asset->TaskMap.Reset();
		AddError(Asset, TEXT("任务图尚未显式迁移，保留原始树供迁移；拒绝编译和运行。"), OutErrors);
		return false;
	}
#endif
	FCompiledTree Tree;
	if(!Build(Asset, Tree, OutErrors) || !Validate(Asset, OutErrors))
	{
		Asset->ClearCompiledTasks();
		return false;
	}
#if WITH_EDITORONLY_DATA
	Asset->ClearCompiledTasks();
	Asset->RootTasks = Tree.Roots;
	Asset->StartTasks = Tree.Roots;
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		Task->GraphChildren.Sort([](const UTaskBase& Left, const UTaskBase& Right)
		{
			if(Left.GraphPosition.Y != Right.GraphPosition.Y)
			{
				return Left.GraphPosition.Y < Right.GraphPosition.Y;
			}
			if(Left.GraphPosition.X != Right.GraphPosition.X)
			{
				return Left.GraphPosition.X < Right.GraphPosition.X;
			}
			return Left.TaskGUID.Compare(Right.TaskGUID, ESearchCase::CaseSensitive) < 0;
		});
		if(const TArray<UTaskBase*>* Children = Tree.Children.Find(Task))
		{
			Task->SubTasks = *Children;
		}
	}
#endif
	Asset->bCompiledValid = Asset->RebuildTaskMap(false);
	return Asset->bCompiledValid;
}
