#include "Task/Graph/TaskGraphOperations.h"

#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"

#define LOCTEXT_NAMESPACE "TaskGraph"

namespace TaskGraphOperations
{
	void Visit(UTaskBase* Task, TFunctionRef<void(UTaskBase*)> Function)
	{
		Function(Task);
		for (UTaskBase* Child : Task->SubTasks) Visit(Child, Function);
	}
	TArray<UTaskBase*> SelectedRoots(const UTaskAsset* Asset, const TArray<UTaskBase*>& Selection)
	{
		TArray<UTaskBase*> Result;
		TFunction<void(UTaskBase*)> Collect = [&](UTaskBase* Task)
		{
			if (Selection.Contains(Task)) Result.Add(Task);
			else for (UTaskBase* Child : Task->SubTasks) Collect(Child);
		};
		for (UTaskBase* Task : Asset->RootTasks) Collect(Task);
		return Result;
	}
}

bool FTaskGraphOperations::CanEdit(const UTaskAsset* Asset)
{
	return Asset && !Asset->SourceObject && GEditor && !GEditor->PlayWorld;
}

UTaskBase* FTaskGraphOperations::AddTask(UTaskAsset* Asset, UClass* TaskClass, FVector2D Position, UTaskBase* Parent, UTaskBase* Child)
{
	if (!CanEdit(Asset) || !TaskClass || !TaskClass->IsChildOf(UTaskBase::StaticClass()) ||
		TaskClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Asset->CanAddTask(TaskClass)) return nullptr;
	if ((Parent && Asset->TaskMap.FindRef(Parent->TaskGUID) != Parent) ||
		(Child && Asset->TaskMap.FindRef(Child->TaskGUID) != Child) || (Parent && Child) || Parent && Parent->IsA<UTaskAssetReferenceTask>()) return nullptr;
	const FScopedTransaction Transaction(LOCTEXT("Add", "Add Task"));
	Asset->Modify();
	UTaskBase* Task = NewObject<UTaskBase>(Asset, TaskClass, NAME_None, RF_Transactional);
	Task->TaskGUID = FGuid::NewGuid().ToString();
	Task->GraphPosition = Position;
	Task->bHasGraphPosition = true;
	TArray<UTaskBase*>* Siblings = &Asset->RootTasks;
	if (Parent)
	{
		Parent->Modify();
		Siblings = &Parent->SubTasks;
	}
	if (Child)
	{
		Child->Modify();
		UTaskBase* PreviousParent = Child->ParentTask;
		if (PreviousParent)
		{
			PreviousParent->Modify();
			Siblings = &PreviousParent->SubTasks;
		}
		const int32 Index = Siblings->IndexOfByKey(Child);
		(*Siblings)[Index] = Task;
		Task->SubTasks.Add(Child);
	}
	else Siblings->Add(Task);
	Task->OnGenerate();
	Asset->RebuildTaskMap(false);
	SortSiblings(Asset, Task);
	return Task;
}

bool FTaskGraphOperations::DeleteTasks(UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks)
{
	if (!CanEdit(Asset)) return false;
	const TArray<UTaskBase*> Roots = TaskGraphOperations::SelectedRoots(Asset, Tasks);
	if (Roots.IsEmpty()) return false;
	const FScopedTransaction Transaction(LOCTEXT("Delete", "Delete Tasks"));
	Asset->Modify();
	TSet<FString> DeletedIDs;
	for (UTaskBase* Task : Roots) TaskGraphOperations::Visit(Task, [&](UTaskBase* Item) { DeletedIDs.Add(Item->TaskGUID); });
	for (const auto& Pair : Asset->TaskMap)
	{
		if (DeletedIDs.Contains(Pair.Key)) continue;
		UTaskBase* Task = Pair.Value;
		if (Task->Prerequisites.ContainsByPredicate([&](const FTaskReference& Ref) { return Ref.Asset.Get() == Asset && DeletedIDs.Contains(Ref.TaskGUID); }))
		{
			Task->Modify();
			Task->Prerequisites.RemoveAll([&](const FTaskReference& Ref) { return Ref.Asset.Get() == Asset && DeletedIDs.Contains(Ref.TaskGUID); });
		}
	}

	for (UTaskBase* Task : Roots)
	{
		if (Task->ParentTask)
		{
			Task->ParentTask->Modify();
			Task->ParentTask->SubTasks.Remove(Task);
		}
		else Asset->RootTasks.Remove(Task);
		TaskGraphOperations::Visit(Task, [](UTaskBase* Item) { Item->Modify(); Item->OnUnGenerate(); });
	}
	Asset->RebuildTaskMap(false);
	return true;
}

UTaskBase* FTaskGraphOperations::ChangeTaskType(UTaskAsset* Asset, UTaskBase* Task, UClass* TaskClass)
{
	if (!CanEdit(Asset) || !Task || Task->GetClass() == TaskClass || !TaskClass ||
		!TaskClass->IsChildOf(UTaskBase::StaticClass()) || TaskClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
		!Asset->CanAddTask(TaskClass) || Asset->TaskMap.FindRef(Task->TaskGUID) != Task ||
		(TaskClass->IsChildOf(UTaskAssetReferenceTask::StaticClass()) && !Task->SubTasks.IsEmpty())) return nullptr;
	const FScopedTransaction Transaction(LOCTEXT("ChangeType", "Change Task Type"));
	Asset->Modify();
	Task->Modify();
	TArray<UTaskBase*>& Siblings = Task->ParentTask ? Task->ParentTask->SubTasks : Asset->RootTasks;
	if (Task->ParentTask) Task->ParentTask->Modify();
	UTaskBase* Replacement = NewObject<UTaskBase>(Asset, TaskClass, NAME_None, RF_Transactional);
	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	Params.bNotifyObjectReplacement = false;
	UEngine::CopyPropertiesForUnrelatedObjects(Task, Replacement, Params);
	Replacement->TaskGUID = Task->TaskGUID;
	Replacement->TaskDisplayName = Task->TaskDisplayName;
	Replacement->TaskDescription = Task->TaskDescription;
	Replacement->SubTasks = Task->SubTasks;
	Replacement->GraphPosition = Task->GraphPosition;
	Replacement->bHasGraphPosition = Task->bHasGraphPosition;
	TaskGraphOperations::Visit(Task, [](UTaskBase* Item) { Item->Modify(); });
	Siblings[Siblings.IndexOfByKey(Task)] = Replacement;
	Task->OnUnGenerate();
	Replacement->OnGenerate();
	Asset->RebuildTaskMap(false);
	return Replacement;
}

bool FTaskGraphOperations::ExportTasks(const UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks, FString& OutText)
{
	if (!Asset) return false;
	const TArray<UTaskBase*> Roots = TaskGraphOperations::SelectedRoots(Asset, Tasks);
	if (Roots.IsEmpty()) return false;
	TArray<TSharedPtr<FJsonValue>> Values;
	TMap<const void*, FVector2D> Positions;
	int32 Row = 0;
	TFunction<void(UTaskBase*, int32)> CollectPositions = [&](UTaskBase* Task, int32 Depth)
	{
		Positions.Add(&Task->GraphPosition, Task->bHasGraphPosition ? Task->GraphPosition : FVector2D(Depth * 320.f, Row * 150.f));
		++Row;
		for (UTaskBase* Child : Task->SubTasks) CollectPositions(Child, Depth + 1);
	};
	for (UTaskBase* Task : Asset->RootTasks) CollectPositions(Task, 0);
	const FJsonObjectConverter::CustomExportCallback Export = FJsonObjectConverter::CustomExportCallback::CreateLambda(
		[&Positions](FProperty* Property, const void* Value) -> TSharedPtr<FJsonValue>
		{
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, GraphPosition))
				if (const FVector2D* Position = Positions.Find(Value))
					return FJsonObjectConverter::UPropertyToJsonValue(Property, Position, 0, CPF_Transient, nullptr, nullptr, EJsonObjectConversionFlags::SkipStandardizeCase);
			if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, ParentTask) || Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, RootTask))
				return MakeShared<FJsonValueString>(TEXT("None"));
			return nullptr;
		});
	for (UTaskBase* Task : Roots)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		if (!FJsonObjectConverter::UStructToJsonObject(Task->GetClass(), Task, Item, 0, CPF_Transient, &Export,
			EJsonObjectConversionFlags::WriteTextAsComplexString | EJsonObjectConversionFlags::SkipStandardizeCase)) return false;
		Item->SetStringField(TEXT("_ClassName"), Task->GetClass()->GetPathName());
		Values.Add(MakeShared<FJsonValueObject>(Item));
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("WHFramework.TaskNodes"));
	Root->SetStringField(TEXT("SourceAsset"), Asset->GetPathName());
	Root->SetArrayField(TEXT("RootTasks"), Values);
	return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&OutText));
}

bool FTaskGraphOperations::CanImportTasks(const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root) return false;
	FString Format;
	const TArray<TSharedPtr<FJsonValue>>* Tasks;
	return Root->TryGetStringField(TEXT("Format"), Format) && Format == TEXT("WHFramework.TaskNodes") &&
		Root->TryGetArrayField(TEXT("RootTasks"), Tasks) && !Tasks->IsEmpty();
}

TArray<UTaskBase*> FTaskGraphOperations::ImportTasks(UTaskAsset* Asset, const FString& Text, FVector2D Position)
{
	if (!CanEdit(Asset) || !CanImportTasks(Text)) return {};
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
	FString SourcePath;
	Root->TryGetStringField(TEXT("SourceAsset"), SourcePath);
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetArrayField(TEXT("RootTasks"), Root->GetArrayField(TEXT("RootTasks")));
	TStrongObjectPtr<UTaskAsset> Imported(NewObject<UTaskAsset>());
	if (!FJsonObjectConverter::JsonObjectToUStruct(Data, UTaskAsset::StaticClass(), Imported.Get(), 0, CPF_Transient)) return {};
	if (!Imported->RebuildTaskMap(false)) return {};
	TArray<UTaskBase*> Tasks;
	TMap<FString, FString> IDs;
	FVector2D Center = FVector2D::ZeroVector;
	for (UTaskBase* Task : Imported->RootTasks)
		TaskGraphOperations::Visit(Task, [&](UTaskBase* Item)
		{
			Tasks.Add(Item);
			IDs.Add(Item->TaskGUID, FGuid::NewGuid().ToString());
			Center += Item->GraphPosition;
		});
	if (Tasks.IsEmpty() || Tasks.ContainsByPredicate([&](UTaskBase* Task) { return !Asset->CanAddTask(Task->GetClass()); })) return {};
	Center /= Tasks.Num();
	const FScopedTransaction Transaction(LOCTEXT("Paste", "Paste Tasks"));
	Asset->Modify();
	const TArray<UTaskBase*> Roots = Imported->RootTasks;
	for (UTaskBase* Task : Tasks)
	{
		Task->Rename(*MakeUniqueObjectName(Asset, Task->GetClass()).ToString(), Asset, REN_DontCreateRedirectors | REN_DoNotDirty | REN_NonTransactional);
		Task->SetFlags(RF_Transactional);
		Task->TaskGUID = IDs[Task->TaskGUID];
		Task->GraphPosition += Position - Center;
		Task->bHasGraphPosition = true;
		Task->ParentTask = nullptr;
		Task->RootTask = nullptr;
		for (FTaskReference& Reference : Task->Prerequisites)
		{
			if (Reference.Asset.ToSoftObjectPath().ToString() == SourcePath && IDs.Contains(Reference.TaskGUID))
			{
				Reference.Asset = Asset;
				Reference.TaskGUID = IDs[Reference.TaskGUID];
			}
		}
		Task->OnGenerate();
	}
	Asset->RootTasks.Append(Roots);
	Asset->RebuildTaskMap(false);
	if(!Roots.IsEmpty()) SortSiblings(Asset, Roots[0]);
	return Roots;
}

void FTaskGraphOperations::AutoLayout(UTaskAsset* Asset)
{
	if (!CanEdit(Asset) || Asset->RootTasks.IsEmpty()) return;
	const FScopedTransaction Transaction(LOCTEXT("AutoLayout", "Auto Layout Tasks"));
	int32 Row = 0;
	for (UTaskBase* Root : Asset->RootTasks)
		TaskGraphOperations::Visit(Root, [&](UTaskBase* Task)
		{
			Task->Modify();
			Task->GraphPosition = FVector2D(Task->TaskHierarchy * 320.f, Row++ * 150.f);
			Task->bHasGraphPosition = true;
		});
}

void FTaskGraphOperations::SortSiblings(UTaskAsset* Asset, UTaskBase* Task)
{
	TArray<UTaskBase*>& Siblings = Task->ParentTask ? Task->ParentTask->SubTasks : Asset->RootTasks;
	if (Task->ParentTask) Task->ParentTask->Modify();
	else Asset->Modify();
	for (UTaskBase* Sibling : Siblings) Sibling->Modify();
	Siblings.StableSort([](const UTaskBase& A, const UTaskBase& B)
	{
		return A.GraphPosition.Y == B.GraphPosition.Y ? A.GraphPosition.X < B.GraphPosition.X : A.GraphPosition.Y < B.GraphPosition.Y;
	});
	Asset->RebuildTaskMap(false);
}

#undef LOCTEXT_NAMESPACE
