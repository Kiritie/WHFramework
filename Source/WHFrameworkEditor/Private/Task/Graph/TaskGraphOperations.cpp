#include "Task/Graph/TaskGraphOperations.h"

#include "Task/Base/TaskAsset.h"
#include "Task/Base/TaskBase.h"
#include "Task/Base/TaskAssetReferenceTask.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "Misc/SecureHash.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

#define LOCTEXT_NAMESPACE "TaskGraph"

namespace TaskGraphOperationsPrivate
{
	void SortByPosition(TArray<UTaskBase*>& Tasks)
	{
		Tasks.Sort([](const UTaskBase& Left, const UTaskBase& Right)
		{
			if(Left.GraphPosition.Y != Right.GraphPosition.Y) return Left.GraphPosition.Y < Right.GraphPosition.Y;
			if(Left.GraphPosition.X != Right.GraphPosition.X) return Left.GraphPosition.X < Right.GraphPosition.X;
			return Left.TaskGUID.Compare(Right.TaskGUID, ESearchCase::CaseSensitive) < 0;
		});
	}

	float GetRowHeight(const TArray<UTaskBase*>& Tasks)
	{
		float Height = 200.f;
		for(const UTaskBase* Task : Tasks)
		{
			if(Task)
			{
				int32 NewlineCount = 0;
				for(const TCHAR Character : Task->TaskDisplayName.ToString())
				{
					if(Character == TEXT('\n'))
					{
						++NewlineCount;
					}
				}
				Height = FMath::Max(Height, 140.f + NewlineCount * 24.f);
			}
		}
		return Height;
	}

	void RecordObjectTree(UObject* Root)
	{
		Root->SetFlags(RF_Transactional);
		Root->Modify();
		ForEachObjectWithOuter(Root, [](UObject* Object)
		{
			Object->SetFlags(RF_Transactional);
			Object->Modify();
		}, true);
	}
}

bool FTaskGraphOperations::CanEdit(const UTaskAsset* Asset)
{
	return Asset && !Asset->SourceObject && (GEditor || IsRunningCommandlet()) && (!GEditor || !GEditor->PlayWorld);
}

void FTaskGraphOperations::ModifyAsset(UTaskAsset* Asset)
{
	Asset->SetFlags(RF_Transactional);
	Asset->Modify();
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		if(Task)
		{
			TaskGraphOperationsPrivate::RecordObjectTree(Task);
		}
	}
}

UTaskBase* FTaskGraphOperations::AddTask(UTaskAsset* Asset, UClass* TaskClass, FVector2D Position, UTaskBase* Parent, UTaskBase* Child, bool bConnectToStart)
{
	if(!CanEdit(Asset) || !Asset->StartGUID.IsValid() || !TaskClass || !TaskClass->IsChildOf(UTaskBase::StaticClass()) ||
		TaskClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Asset->CanAddTask(TaskClass) ||
		(Parent && Asset->FindAuthoredTask(Parent->TaskGUID) != Parent) || (Child && Asset->FindAuthoredTask(Child->TaskGUID) != Child) ||
		(Parent && Child) || (bConnectToStart && (Parent || Child)) || (Parent && Parent->IsA<UTaskAssetReferenceTask>()) ||
		(Child && TaskClass->IsChildOf(UTaskAssetReferenceTask::StaticClass())))
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(LOCTEXT("Add", "Add Task"));
	ModifyAsset(Asset);
	UTaskBase* Task = NewObject<UTaskBase>(Asset, TaskClass, NAME_None, RF_Transactional);
	TaskGraphOperationsPrivate::RecordObjectTree(Task);
	Task->TaskGUID = FGuid::NewGuid().ToString();
	Task->GraphPosition = Position;
	Task->bHasGraphPosition = true;
	Task->GraphChildren.Reset();
	Task->SubTasks.Reset();
	Asset->AuthoredTasks.Add(Task);
	if(Parent)
	{
		Parent->GraphChildren.Add(Task);
	}
	else if(bConnectToStart)
	{
		Asset->StartTasks.Add(Task);
	}
	else if(Child)
	{
		if(UTaskBase* PreviousParent = Asset->FindGraphParent(Child))
		{
			PreviousParent->GraphChildren[PreviousParent->GraphChildren.IndexOfByKey(Child)] = Task;
		}
		else if(const int32 Index = Asset->StartTasks.IndexOfByKey(Child); Index != INDEX_NONE)
		{
			Asset->StartTasks[Index] = Task;
		}
		Task->GraphChildren.Add(Child);
	}
	Task->OnGenerate();
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
	return Task;
}

bool FTaskGraphOperations::CanConnect(const UTaskAsset* Asset, UTaskBase* Parent, UTaskBase* Child, bool bFromStart, FText& OutReason)
{
	if(!CanEdit(Asset) || !Asset->StartGUID.IsValid() || !Child || Asset->FindAuthoredTask(Child->TaskGUID) != Child ||
		(bFromStart ? Parent != nullptr : !Parent || Asset->FindAuthoredTask(Parent->TaskGUID) != Parent))
	{
		OutReason = LOCTEXT("InvalidConnection", "The graph or endpoint is invalid.");
		return false;
	}
	if(Parent == Child || (Parent && Parent->IsA<UTaskAssetReferenceTask>()))
	{
		OutReason = LOCTEXT("InvalidParent", "A task cannot parent itself; asset references cannot have authored children.");
		return false;
	}
	if(Asset->StartTasks.Contains(Child) || Asset->FindGraphParent(Child))
	{
		OutReason = LOCTEXT("MultipleParents", "Disconnect the existing parent before connecting another parent.");
		return false;
	}
	TSet<UTaskBase*> Visited;
	TArray<UTaskBase*> Pending = {Child};
	while(!Pending.IsEmpty())
	{
		UTaskBase* Current = Pending.Pop(EAllowShrinking::No);
		if(Current == Parent)
		{
			OutReason = LOCTEXT("Cycle", "This connection creates a cycle.");
			return false;
		}
		if(Current && !Visited.Contains(Current))
		{
			Visited.Add(Current);
			Pending.Append(Current->GraphChildren);
		}
	}
	OutReason = LOCTEXT("Connect", "Connect parent Out to child In.");
	return true;
}

bool FTaskGraphOperations::Connect(UTaskAsset* Asset, UTaskBase* Parent, UTaskBase* Child, bool bFromStart)
{
	FText Reason;
	if(!CanConnect(Asset, Parent, Child, bFromStart, Reason))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("ConnectTransaction", "Connect Tasks"));
	ModifyAsset(Asset);
	(bFromStart ? Asset->StartTasks : Parent->GraphChildren).Add(Child);
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
	return true;
}

bool FTaskGraphOperations::Disconnect(UTaskAsset* Asset, UTaskBase* Task, bool bInput, bool bOutput, bool bStartNode)
{
	if(!CanEdit(Asset) || (!bStartNode && (!Task || Asset->FindAuthoredTask(Task->TaskGUID) != Task)))
	{
		return false;
	}
	UTaskBase* Parent = bStartNode ? nullptr : Asset->FindGraphParent(Task);
	const bool bHasInput = !bStartNode && bInput && (Parent || Asset->StartTasks.Contains(Task));
	const bool bHasOutput = bOutput && (bStartNode ? !Asset->StartTasks.IsEmpty() : !Task->GraphChildren.IsEmpty());
	if(!bHasInput && !bHasOutput)
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("DisconnectTransaction", "Disconnect Tasks"));
	ModifyAsset(Asset);
	if(bHasInput)
	{
		Asset->StartTasks.Remove(Task);
		if(Parent)
		{
			Parent->GraphChildren.Remove(Task);
		}
	}
	if(bHasOutput)
	{
		(bStartNode ? Asset->StartTasks : Task->GraphChildren).Reset();
	}
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
	return true;
}

bool FTaskGraphOperations::DisconnectEdge(UTaskAsset* Asset, UTaskBase* Parent, UTaskBase* Child, bool bFromStart)
{
	if(!CanEdit(Asset) || !Child || Asset->FindAuthoredTask(Child->TaskGUID) != Child ||
		(!bFromStart && (!Parent || Asset->FindAuthoredTask(Parent->TaskGUID) != Parent)))
	{
		return false;
	}
	TArray<UTaskBase*>& Children = bFromStart ? Asset->StartTasks : Parent->GraphChildren;
	if(!Children.Contains(Child))
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("DisconnectEdge", "Disconnect Task Link"));
	ModifyAsset(Asset);
	Children.Remove(Child);
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
	return true;
}

bool FTaskGraphOperations::DeleteTasks(UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks)
{
	if(!CanEdit(Asset))
	{
		return false;
	}
	TSet<UTaskBase*> Deleted;
	TSet<FString> DeletedIDs;
	for(UTaskBase* Task : Tasks)
	{
		if(Task && Asset->FindAuthoredTask(Task->TaskGUID) == Task)
		{
			Deleted.Add(Task);
			DeletedIDs.Add(Task->TaskGUID);
		}
	}
	if(Deleted.IsEmpty())
	{
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("Delete", "Delete Tasks"));
	ModifyAsset(Asset);
	Asset->StartTasks.RemoveAll([&](UTaskBase* Task) { return Deleted.Contains(Task); });
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		Task->GraphChildren.RemoveAll([&](UTaskBase* Child) { return Deleted.Contains(Child); });
		Task->Prerequisites.RemoveAll([&](const FTaskReference& Reference)
		{
			return Reference.Asset.ToSoftObjectPath() == FSoftObjectPath(Asset) && DeletedIDs.Contains(Reference.TaskGUID);
		});
		if(Deleted.Contains(Task))
		{
			Task->OnUnGenerate();
			Task->GraphChildren.Reset();
			Task->SubTasks.Reset();
			Task->ParentTask = nullptr;
			Task->RootTask = nullptr;
		}
	}
	Asset->AuthoredTasks.RemoveAll([&](UTaskBase* Task) { return Deleted.Contains(Task); });
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
	return true;
}

UTaskBase* FTaskGraphOperations::ChangeTaskType(UTaskAsset* Asset, UTaskBase* Task, UClass* TaskClass)
{
	if(!CanEdit(Asset) || !Task || !TaskClass || Task->GetClass() == TaskClass || !TaskClass->IsChildOf(UTaskBase::StaticClass()) ||
		TaskClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Asset->CanAddTask(TaskClass) ||
		Asset->FindAuthoredTask(Task->TaskGUID) != Task || (TaskClass->IsChildOf(UTaskAssetReferenceTask::StaticClass()) && !Task->GraphChildren.IsEmpty()))
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(LOCTEXT("ChangeType", "Change Task Type"));
	ModifyAsset(Asset);
	UTaskBase* Replacement = NewObject<UTaskBase>(Asset, TaskClass, NAME_None, RF_Transactional);
	UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
	Params.bNotifyObjectReplacement = false;
	UEngine::CopyPropertiesForUnrelatedObjects(Task, Replacement, Params);
	TaskGraphOperationsPrivate::RecordObjectTree(Replacement);
	Replacement->TaskGUID = Task->TaskGUID;
	Replacement->GraphChildren = Task->GraphChildren;
	Replacement->GraphPosition = Task->GraphPosition;
	Replacement->bHasGraphPosition = Task->bHasGraphPosition;
	Asset->AuthoredTasks[Asset->AuthoredTasks.IndexOfByKey(Task)] = Replacement;
	for(UTaskBase*& Root : Asset->StartTasks)
	{
		if(Root == Task)
		{
			Root = Replacement;
		}
	}
	for(UTaskBase* Other : Asset->AuthoredTasks)
	{
		for(UTaskBase*& Child : Other->GraphChildren)
		{
			if(Child == Task)
			{
				Child = Replacement;
			}
		}
	}
	Task->OnUnGenerate();
	Replacement->OnGenerate();
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
	return Replacement;
}

bool FTaskGraphOperations::ExportTasks(const UTaskAsset* Asset, const TArray<UTaskBase*>& Tasks, FString& OutText)
{
	OutText.Reset();
	if(!Asset)
	{
		return false;
	}
	TSet<UTaskBase*> Selection;
	for(UTaskBase* Task : Tasks)
	{
		if(Task && Asset->FindAuthoredTask(Task->TaskGUID) == Task)
		{
			Selection.Add(Task);
		}
	}
	if(Selection.IsEmpty())
	{
		return false;
	}
	const FJsonObjectConverter::CustomExportCallback Export = FJsonObjectConverter::CustomExportCallback::CreateLambda(
		[](FProperty* Property, const void* Value) -> TSharedPtr<FJsonValue>
		{
			if(Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, GraphChildren) || Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, SubTasks))
			{
				return MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>());
			}
			if(Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, ParentTask) || Property->GetFName() == GET_MEMBER_NAME_CHECKED(UTaskBase, RootTask))
			{
				return MakeShared<FJsonValueString>(TEXT("None"));
			}
			return nullptr;
		});
	TArray<TSharedPtr<FJsonValue>> Nodes;
	TArray<TSharedPtr<FJsonValue>> Links;
	for(UTaskBase* Task : Asset->AuthoredTasks)
	{
		if(!Selection.Contains(Task))
		{
			continue;
		}
		TSharedRef<FJsonObject> Node = MakeShared<FJsonObject>();
		if(!FJsonObjectConverter::UStructToJsonObject(Task->GetClass(), Task, Node, 0, CPF_Transient, &Export,
			EJsonObjectConversionFlags::WriteTextAsComplexString | EJsonObjectConversionFlags::SkipStandardizeCase))
		{
			return false;
		}
		Node->SetStringField(TEXT("_ClassName"), Task->GetClass()->GetPathName());
		Nodes.Add(MakeShared<FJsonValueObject>(Node));
		for(UTaskBase* Child : Task->GraphChildren)
		{
			if(Selection.Contains(Child))
			{
				TSharedRef<FJsonObject> Link = MakeShared<FJsonObject>();
				Link->SetStringField(TEXT("Parent"), Task->TaskGUID);
				Link->SetStringField(TEXT("Child"), Child->TaskGUID);
				Links.Add(MakeShared<FJsonValueObject>(Link));
			}
		}
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("WHFramework.TaskGraph.2"));
	Root->SetStringField(TEXT("SourceAsset"), Asset->GetPathName());
	Root->SetArrayField(TEXT("Nodes"), Nodes);
	Root->SetArrayField(TEXT("Links"), Links);
	return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&OutText));
}

bool FTaskGraphOperations::CanImportTasks(const FString& Text)
{
	if(Text.Len() > 16 * 1024 * 1024)
	{
		return false;
	}
	TSharedPtr<FJsonObject> Root;
	if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		return false;
	}
	FString Format;
	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
	return Root->TryGetStringField(TEXT("Format"), Format) && Format == TEXT("WHFramework.TaskGraph.2") &&
		Root->TryGetArrayField(TEXT("Nodes"), Nodes) && !Nodes->IsEmpty() && Nodes->Num() <= 4096 &&
		Root->TryGetArrayField(TEXT("Links"), Links) && Links->Num() <= Nodes->Num();
}

TArray<UTaskBase*> FTaskGraphOperations::ImportTasks(UTaskAsset* Asset, const FString& Text, FVector2D Position)
{
	if(!CanEdit(Asset) || !Asset->StartGUID.IsValid() || !CanImportTasks(Text))
	{
		return {};
	}
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
	FString SourcePath;
	Root->TryGetStringField(TEXT("SourceAsset"), SourcePath);
	TStrongObjectPtr<UTaskAsset> Imported(NewObject<UTaskAsset>());
	Imported->InitializeGraph();
	TMap<FString, UTaskBase*> ImportedByID;
	FVector2D Center = FVector2D::ZeroVector;
	for(const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Nodes")))
	{
		const TSharedPtr<FJsonObject>* Node = nullptr;
		FString ClassPath;
		if(!Value || !Value->TryGetObject(Node) || !Node || !(*Node)->TryGetStringField(TEXT("_ClassName"), ClassPath))
		{
			return {};
		}
		UClass* TaskClass = LoadClass<UTaskBase>(nullptr, *ClassPath);
		if(!TaskClass || TaskClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Asset->CanAddTask(TaskClass))
		{
			return {};
		}
		UTaskBase* Task = NewObject<UTaskBase>(Imported.Get(), TaskClass);
		if(!FJsonObjectConverter::JsonObjectToUStruct((*Node).ToSharedRef(), TaskClass, Task, 0, CPF_Transient) ||
			Task->TaskGUID.IsEmpty() || ImportedByID.Contains(Task->TaskGUID))
		{
			return {};
		}
		Task->GraphChildren.Reset();
		Task->SubTasks.Reset();
		Task->ParentTask = nullptr;
		Task->RootTask = nullptr;
		Imported->AuthoredTasks.Add(Task);
		ImportedByID.Add(Task->TaskGUID, Task);
		Center += Task->GraphPosition;
	}
	for(const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Links")))
	{
		const TSharedPtr<FJsonObject>* Link = nullptr;
		FString ParentID;
		FString ChildID;
		if(!Value || !Value->TryGetObject(Link) || !Link || !(*Link)->TryGetStringField(TEXT("Parent"), ParentID) ||
			!(*Link)->TryGetStringField(TEXT("Child"), ChildID) || !ImportedByID.Contains(ParentID) || !ImportedByID.Contains(ChildID))
		{
			return {};
		}
		ImportedByID.FindChecked(ParentID)->GraphChildren.Add(ImportedByID.FindChecked(ChildID));
	}
	TArray<FText> Errors;
	if(!Imported->CompileTasks(Errors))
	{
		return {};
	}
	Center /= Imported->AuthoredTasks.Num();
	TMap<UTaskBase*, UTaskBase*> Clones;
	TMap<FString, FString> NewIDs;
	TArray<UTaskBase*> Added;
	for(UTaskBase* Task : Imported->AuthoredTasks)
	{
		UTaskBase* Clone = DuplicateObject<UTaskBase>(Task, Asset, MakeUniqueObjectName(Asset, Task->GetClass()));
		if(!Clone)
		{
			return {};
		}
		Clone->SetFlags(RF_Transactional);
		Clone->TaskGUID = FGuid::NewGuid().ToString();
		Clone->GraphPosition = Task->GraphPosition + Position - Center;
		Clone->bHasGraphPosition = true;
		Clone->GraphChildren.Reset();
		Clone->SubTasks.Reset();
		Clone->ParentTask = nullptr;
		Clone->RootTask = nullptr;
		Clones.Add(Task, Clone);
		NewIDs.Add(Task->TaskGUID, Clone->TaskGUID);
		Added.Add(Clone);
	}
	for(UTaskBase* Task : Imported->AuthoredTasks)
	{
		UTaskBase* Clone = Clones.FindChecked(Task);
		for(UTaskBase* Child : Task->GraphChildren)
		{
			Clone->GraphChildren.Add(Clones.FindChecked(Child));
		}
		for(FTaskReference& Reference : Clone->Prerequisites)
		{
			if(Reference.Asset.ToSoftObjectPath().ToString() == SourcePath)
			{
				if(const FString* NewID = NewIDs.Find(Reference.TaskGUID))
				{
					Reference.Asset = Asset;
					Reference.TaskGUID = *NewID;
					Reference.InstanceID.Invalidate();
				}
			}
		}
	}
	const FScopedTransaction Transaction(LOCTEXT("Paste", "Paste Tasks"));
	ModifyAsset(Asset);
	for(UTaskBase* Task : Added)
	{
		Task->Modify();
		ForEachObjectWithOuter(Task, [](UObject* Object) { Object->SetFlags(RF_Transactional); Object->Modify(); }, true);
		Task->OnGenerate();
	}
	Asset->AuthoredTasks.Append(Added);
	Errors.Reset();
	Asset->CompileTasks(Errors);
	return Added;
}

void FTaskGraphOperations::SetNodePosition(UTaskAsset* Asset, UTaskBase* Task, bool bStartNode, FVector2D Position)
{
	if(!CanEdit(Asset) || (!bStartNode && (!Task || Asset->FindAuthoredTask(Task->TaskGUID) != Task)))
	{
		return;
	}
	if(bStartNode ? Asset->StartPosition == Position : Task->GraphPosition == Position)
	{
		return;
	}
	ModifyAsset(Asset);
	if(bStartNode)
	{
		Asset->StartPosition = Position;
	}
	else
	{
		Task->GraphPosition = Position;
		Task->bHasGraphPosition = true;
		TArray<FText> Errors;
		Asset->CompileTasks(Errors);
	}
}

void FTaskGraphOperations::SortSiblings(UTaskAsset* Asset, UTaskBase* Task)
{
	if(!CanEdit(Asset) || !Task || Asset->FindAuthoredTask(Task->TaskGUID) != Task)
	{
		return;
	}
	ModifyAsset(Asset);
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
}

void FTaskGraphOperations::AutoLayout(UTaskAsset* Asset)
{
	if(!CanEdit(Asset) || Asset->AuthoredTasks.IsEmpty())
	{
		return;
	}
	TMap<UTaskBase*, FVector2D> Positions;
	TSet<UTaskBase*> Visited;
	const float RowHeight = TaskGraphOperationsPrivate::GetRowHeight(Asset->AuthoredTasks);
	int32 Row = 0;
	TFunction<void(UTaskBase*, int32)> Place = [&](UTaskBase* Task, int32 Depth)
	{
		if(!Task || Visited.Contains(Task))
		{
			return;
		}
		Visited.Add(Task);
		Positions.Add(Task, FVector2D(Depth * 400.f, Row++ * RowHeight));
		TArray<UTaskBase*> Children = Task->GraphChildren;
		TaskGraphOperationsPrivate::SortByPosition(Children);
		for(UTaskBase* Child : Children)
		{
			Place(Child, Depth + 1);
		}
	};
	TArray<UTaskBase*> Roots = Asset->StartTasks;
	TaskGraphOperationsPrivate::SortByPosition(Roots);
	for(UTaskBase* Root : Roots)
	{
		Place(Root, 0);
	}
	TArray<UTaskBase*> Authored = Asset->AuthoredTasks;
	TaskGraphOperationsPrivate::SortByPosition(Authored);
	for(UTaskBase* Task : Authored)
	{
		if(!Asset->FindGraphParent(Task))
		{
			Place(Task, 0);
		}
	}
	for(UTaskBase* Task : Authored)
	{
		Place(Task, 0);
	}
	bool bChanged = Asset->StartPosition != FVector2D(-400.f, 0.f);
	for(const TPair<UTaskBase*, FVector2D>& Pair : Positions)
	{
		bChanged |= Pair.Key->GraphPosition != Pair.Value || !Pair.Key->bHasGraphPosition;
	}
	if(!bChanged)
	{
		return;
	}
	const FScopedTransaction Transaction(LOCTEXT("AutoLayout", "Auto Layout Tasks"));
	ModifyAsset(Asset);
	Asset->StartPosition = FVector2D(-400.f, 0.f);
	for(const TPair<UTaskBase*, FVector2D>& Pair : Positions)
	{
		Pair.Key->GraphPosition = Pair.Value;
		Pair.Key->bHasGraphPosition = true;
	}
	TArray<FText> Errors;
	Asset->CompileTasks(Errors);
}

bool FTaskGraphOperations::MigrateAsset(UTaskAsset* Asset, TArray<FText>& OutErrors, bool bCompile)
{
	if(!CanEdit(Asset) || Asset->StartGUID.IsValid())
	{
		return false;
	}
	const TArray<UTaskBase*> OriginalRoots = Asset->RootTasks;
	TArray<UTaskBase*> OriginalTasks;
	TMap<UTaskBase*, TArray<UTaskBase*>> OriginalChildren;
	TMap<UTaskBase*, int32> Depths;
	TSet<FGuid> GUIDs;
	TFunction<bool(UTaskBase*, int32)> Capture = [&](UTaskBase* Task, int32 Depth)
	{
		FGuid ParsedGUID;
		if(Task && !FGuid::Parse(Task->TaskGUID, ParsedGUID))
		{
			FGuid::ParseExact(FMD5::HashAnsiString(*Task->TaskGUID), EGuidFormats::Digits, ParsedGUID);
		}
		if(!Task || Task->GetTaskAsset() != Asset || OriginalChildren.Contains(Task) || Task->TaskGUID.IsEmpty() ||
			!ParsedGUID.IsValid() || GUIDs.Contains(ParsedGUID))
		{
			OutErrors.Add(LOCTEXT("MigrationInvalidTree", "The original task tree contains invalid identities, shared nodes, or a cycle."));
			return false;
		}
		GUIDs.Add(ParsedGUID);
		OriginalTasks.Add(Task);
		OriginalChildren.Add(Task, Task->SubTasks);
		Depths.Add(Task, Depth);
		for(UTaskBase* Child : Task->SubTasks)
		{
			if(!Capture(Child, Depth + 1))
			{
				return false;
			}
		}
		return true;
	};
	for(UTaskBase* Root : OriginalRoots)
	{
		if(!Capture(Root, 0))
		{
			return false;
		}
	}
	const FScopedTransaction Transaction(LOCTEXT("MigrateGraph", "Create Start Task Graph"));
	Asset->Modify();
	Asset->StartGUID = FGuid::NewGuid();
	Asset->StartPosition = FVector2D(-400.f, 0.f);
	Asset->AuthoredTasks = OriginalTasks;
	Asset->StartTasks = OriginalRoots;
	const float RowHeight = TaskGraphOperationsPrivate::GetRowHeight(OriginalTasks);
	for(int32 Index = 0; Index < OriginalTasks.Num(); ++Index)
	{
		UTaskBase* Task = OriginalTasks[Index];
		Task->SetFlags(RF_Transactional);
		Task->Modify();
		Task->GraphChildren = OriginalChildren.FindChecked(Task);
		Task->GraphPosition = FVector2D(Depths.FindChecked(Task) * 400.f, Index * RowHeight);
		Task->bHasGraphPosition = true;
	}
	if(bCompile)
	{
		Asset->CompileTasks(OutErrors);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
