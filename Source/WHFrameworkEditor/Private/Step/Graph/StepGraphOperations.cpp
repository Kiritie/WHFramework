#include "Step/Graph/StepGraphOperations.h"

#include "Step/Base/StepAsset.h"
#include "Step/Base/StepBase.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "Step/Graph/StepAssetGraph.h"
#include "EdGraph/EdGraphNode.h"

namespace StepGraphOperationsPrivate
{
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

bool FStepGraphOperations::CanEdit(const UStepAsset* Asset)
{
	return Asset && !Asset->SourceObject && (GEditor || IsRunningCommandlet()) && (!GEditor || !GEditor->PlayWorld);
}

void FStepGraphOperations::Compile(UStepAsset* Asset)
{
	TArray<FText> Errors;
	Asset->Compile(Errors);
	Asset->MarkPackageDirty();
}

bool FStepGraphOperations::CanConnect(const UStepAsset* Asset, const FString& ParentGUID, const FString& ChildGUID, FText& OutReason)
{
	if(!Asset || ParentGUID == ChildGUID || ChildGUID == Asset->StartGUID || !Asset->FindDefinition(ChildGUID) || (ParentGUID != Asset->StartGUID && !Asset->FindDefinition(ParentGUID)))
	{
		OutReason = NSLOCTEXT("StepGraph", "InvalidLink", "Start has no input; the child and parent must be valid, different nodes.");
		return false;
	}
	if(Asset->Links.ContainsByPredicate([&ChildGUID](const FStepGraphLink& Link){ return Link.ChildGUID == ChildGUID; }))
	{
		OutReason = NSLOCTEXT("StepGraph", "SingleParent", "Disconnect the existing input before assigning another parent.");
		return false;
	}
	TArray<FString> Pending;
	TSet<FString> Visited;
	Pending.Add(ChildGUID);
	while(!Pending.IsEmpty())
	{
		const FString GUID = Pending.Pop(EAllowShrinking::No);
		if(GUID == ParentGUID)
		{
			OutReason = NSLOCTEXT("StepGraph", "Cycle", "The connection would create a cycle.");
			return false;
		}
		if(Visited.Contains(GUID))
		{
			continue;
		}
		Visited.Add(GUID);
		for(const FStepGraphLink& Link : Asset->Links)
		{
			if(Link.ParentGUID == GUID)
			{
				Pending.Add(Link.ChildGUID);
			}
		}
	}
	OutReason = FText::GetEmpty();
	return true;
}

bool FStepGraphOperations::Connect(UStepAsset* Asset, const FString& ParentGUID, const FString& ChildGUID)
{
	FText Reason;
	if(!CanEdit(Asset) || !CanConnect(Asset, ParentGUID, ChildGUID, Reason))
	{
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "ConnectTransaction", "Connect Steps"));
	ModifyForEdit(Asset);
	FStepGraphLink Link;
	Link.ParentGUID = ParentGUID;
	Link.ChildGUID = ChildGUID;
	Asset->Links.Add(Link);
	Compile(Asset);
	return true;
}

bool FStepGraphOperations::Disconnect(UStepAsset* Asset, const FString& GUID, bool bInput, bool bOutput)
{
	const auto Matches = [&GUID, bInput, bOutput](const FStepGraphLink& Link)
	{
		return (bInput && Link.ChildGUID == GUID) || (bOutput && Link.ParentGUID == GUID);
	};
	if(!CanEdit(Asset) || !Asset->Links.ContainsByPredicate(Matches))
	{
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "DisconnectTransaction", "Disconnect Steps"));
	ModifyForEdit(Asset);
	Asset->Links.RemoveAll(Matches);
	Compile(Asset);
	return true;
}

bool FStepGraphOperations::DisconnectEdge(UStepAsset* Asset, const FString& ParentGUID, const FString& ChildGUID)
{
	const auto Matches = [&ParentGUID, &ChildGUID](const FStepGraphLink& Link)
	{
		return Link.ParentGUID == ParentGUID && Link.ChildGUID == ChildGUID;
	};
	if(!CanEdit(Asset) || !Asset->Links.ContainsByPredicate(Matches))
	{
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "DisconnectEdgeTransaction", "Disconnect Step Edge"));
	ModifyForEdit(Asset);
	Asset->Links.RemoveAll(Matches);
	Compile(Asset);
	return true;
}

UStepBase* FStepGraphOperations::AddStep(UStepAsset* Asset, UClass* StepClass, FVector2D Position, const FString& ParentGUID, const FString& ChildGUID)
{
	if(!CanEdit(Asset) || !StepClass || !StepClass->IsChildOf(UStepBase::StaticClass()) || !Asset->CanAddStep(StepClass) || (!ParentGUID.IsEmpty() && !ChildGUID.IsEmpty()))
	{
		return nullptr;
	}
	if((!ParentGUID.IsEmpty() && ParentGUID != Asset->StartGUID && !Asset->FindDefinition(ParentGUID)) || (!ChildGUID.IsEmpty() && !Asset->FindDefinition(ChildGUID)))
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "AddTransaction", "Add Step"));
	ModifyForEdit(Asset);
	UStepBase* Step = NewObject<UStepBase>(Asset, StepClass, NAME_None, RF_Transactional);
	StepGraphOperationsPrivate::RecordObjectTree(Step);
	Step->StepGUID = FGuid::NewGuid().ToString();
	Step->GraphPosition = Position;
	Asset->Steps.Add(Step);
	if(!ParentGUID.IsEmpty())
	{
		FStepGraphLink Link;
		Link.ParentGUID = ParentGUID;
		Link.ChildGUID = Step->StepGUID;
		Asset->Links.Add(Link);
	}
	if(!ChildGUID.IsEmpty())
	{
		for(FStepGraphLink& Link : Asset->Links)
		{
			if(Link.ChildGUID == ChildGUID)
			{
				Link.ChildGUID = Step->StepGUID;
				break;
			}
		}
		FStepGraphLink Link;
		Link.ParentGUID = Step->StepGUID;
		Link.ChildGUID = ChildGUID;
		Asset->Links.Add(Link);
	}
	Step->OnGenerate();
	Compile(Asset);
	return Step;
}

bool FStepGraphOperations::DeleteSteps(UStepAsset* Asset, const TArray<UStepBase*>& Steps)
{
	if(!CanEdit(Asset))
	{
		return false;
	}
	TSet<FString> Deleted;
	for(UStepBase* Step : Steps)
	{
		if(Step && Asset->FindDefinition(Step->StepGUID) == Step)
		{
			Deleted.Add(Step->StepGUID);
		}
	}
	if(Deleted.IsEmpty())
	{
		return false;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "DeleteTransaction", "Delete Steps"));
	ModifyForEdit(Asset);
	for(UStepBase* Step : Asset->Steps)
	{
		if(Deleted.Contains(Step->StepGUID))
		{
			Step->Modify();
			Step->OnUnGenerate();
		}
	}
	Asset->Links.RemoveAll([&Deleted](const FStepGraphLink& Link){ return Deleted.Contains(Link.ParentGUID) || Deleted.Contains(Link.ChildGUID); });
	Asset->Steps.RemoveAll([&Deleted](const UStepBase* Step){ return Deleted.Contains(Step->StepGUID); });
	Compile(Asset);
	return true;
}

UStepBase* FStepGraphOperations::ChangeStepType(UStepAsset* Asset, UStepBase* Step, UClass* StepClass)
{
	if(!CanEdit(Asset) || !Step || !StepClass || Step->GetClass() == StepClass || !StepClass->IsChildOf(UStepBase::StaticClass()) || !Asset->CanAddStep(StepClass) || Asset->FindDefinition(Step->StepGUID) != Step)
	{
		return nullptr;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "ChangeTypeTransaction", "Change Step Type"));
	ModifyForEdit(Asset);
	Step->Modify();
	UStepBase* Replacement = NewObject<UStepBase>(Asset, StepClass, NAME_None, RF_Transactional);
	Replacement->Modify();
	UEngine::FCopyPropertiesForUnrelatedObjectsParams Parameters;
	Parameters.bNotifyObjectReplacement = false;
	UEngine::CopyPropertiesForUnrelatedObjects(Step, Replacement, Parameters);
	StepGraphOperationsPrivate::RecordObjectTree(Replacement);
	Replacement->StepGUID = Step->StepGUID;
	Replacement->GraphPosition = Step->GraphPosition;
	Replacement->Runtime = nullptr;
	Replacement->ParentStep = nullptr;
	Replacement->RootStep = nullptr;
	Replacement->SubSteps.Reset();
	Asset->Steps[Asset->Steps.IndexOfByKey(Step)] = Replacement;
	Step->OnUnGenerate();
	Replacement->OnGenerate();
	Compile(Asset);
	return Replacement;
}

bool FStepGraphOperations::ExportSteps(const UStepAsset* Asset, const TArray<UStepBase*>& Steps, FString& OutText)
{
	OutText.Reset();
	if(!Asset)
	{
		return false;
	}
	TSet<FString> Selection;
	for(const UStepBase* Step : Steps)
	{
		if(Step && Asset->FindDefinition(Step->StepGUID) == Step)
		{
			Selection.Add(Step->StepGUID);
		}
	}
	if(Selection.IsEmpty())
	{
		return false;
	}
	TArray<TSharedPtr<FJsonValue>> Nodes;
	TArray<TSharedPtr<FJsonValue>> Links;
	TArray<TSharedPtr<FJsonValue>> Roots;
	TSet<FString> ChildGUIDs;
	for(const FStepGraphLink& Link : Asset->Links)
	{
		if(Selection.Contains(Link.ParentGUID) && Selection.Contains(Link.ChildGUID))
		{
			TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
			Object->SetStringField(TEXT("ParentGUID"), Link.ParentGUID);
			Object->SetStringField(TEXT("ChildGUID"), Link.ChildGUID);
			Links.Add(MakeShared<FJsonValueObject>(Object));
			ChildGUIDs.Add(Link.ChildGUID);
		}
	}
	TArray<FString> GUIDs = Selection.Array();
	GUIDs.Sort();
	for(const FString& GUID : GUIDs)
	{
		UStepBase* Step = Asset->FindDefinition(GUID);
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		if(!FJsonObjectConverter::UStructToJsonObject(Step->GetClass(), Step, Properties, 0, CPF_Transient | CPF_DuplicateTransient, nullptr, EJsonObjectConversionFlags::WriteTextAsComplexString | EJsonObjectConversionFlags::SkipStandardizeCase))
		{
			return false;
		}
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(TEXT("GUID"), GUID);
		Object->SetStringField(TEXT("Class"), Step->GetClass()->GetPathName());
		Object->SetObjectField(TEXT("Properties"), Properties);
		Nodes.Add(MakeShared<FJsonValueObject>(Object));
		if(!ChildGUIDs.Contains(GUID))
		{
			Roots.Add(MakeShared<FJsonValueString>(GUID));
		}
	}
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("Format"), TEXT("WHFramework.StepNodes.1"));
	Root->SetStringField(TEXT("SourceAsset"), Asset->GetPathName());
	Root->SetArrayField(TEXT("Nodes"), Nodes);
	Root->SetArrayField(TEXT("Links"), Links);
	Root->SetArrayField(TEXT("RootGUIDs"), Roots);
	return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&OutText));
}

bool FStepGraphOperations::CanImportSteps(const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	if(Text.Len() > 32 * 1024 * 1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
	{
		return false;
	}
	FString Format;
	const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
	return Root->TryGetStringField(TEXT("Format"), Format) && Format == TEXT("WHFramework.StepNodes.1") && Root->TryGetArrayField(TEXT("Nodes"), Nodes) && !Nodes->IsEmpty() && Nodes->Num() <= 4096;
}

TArray<UStepBase*> FStepGraphOperations::ImportSteps(UStepAsset* Asset, const FString& Text, FVector2D Position, bool bPreserveGUIDs, bool bConnectRoots)
{
	if(!CanEdit(Asset) || !CanImportSteps(Text))
	{
		return {};
	}
	TSharedPtr<FJsonObject> Root;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root);
	const TArray<TSharedPtr<FJsonValue>>* SourceLinks = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* SourceRoots = nullptr;
	if(!Root->TryGetArrayField(TEXT("Links"), SourceLinks) || !Root->TryGetArrayField(TEXT("RootGUIDs"), SourceRoots))
	{
		return {};
	}
	TStrongObjectPtr<UStepAsset> Staging(NewObject<UStepAsset>());
	TMap<FString, FString> GUIDMap;
	TSet<FGuid> SourceIDs;
	TSet<FGuid> DestinationIDs;
	FGuid DestinationGUID;
	FGuid::Parse(Asset->StartGUID, DestinationGUID);
	DestinationIDs.Add(DestinationGUID);
	for(const UStepBase* Existing : Asset->Steps)
	{
		if(Existing && FGuid::Parse(Existing->StepGUID, DestinationGUID))
		{
			DestinationIDs.Add(DestinationGUID);
		}
	}
	FVector2D Center = FVector2D::ZeroVector;
	for(const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Nodes")))
	{
		const TSharedPtr<FJsonObject>* Node = nullptr;
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		FString GUID;
		FString ClassPath;
		FGuid ParsedGUID;
		if(!Value.IsValid() || !Value->TryGetObject(Node) || !(*Node)->TryGetStringField(TEXT("GUID"), GUID) || !(*Node)->TryGetStringField(TEXT("Class"), ClassPath) || !(*Node)->TryGetObjectField(TEXT("Properties"), Properties) || !FGuid::Parse(GUID, ParsedGUID) || !ParsedGUID.IsValid() || GUIDMap.Contains(GUID) || SourceIDs.Contains(ParsedGUID))
		{
			return {};
		}
		SourceIDs.Add(ParsedGUID);
		UClass* StepClass = LoadObject<UClass>(nullptr, *ClassPath);
		if(!StepClass || !StepClass->IsChildOf(UStepBase::StaticClass()) || !Asset->CanAddStep(StepClass))
		{
			return {};
		}
		UStepBase* Step = NewObject<UStepBase>(Staging.Get(), StepClass);
		if(!FJsonObjectConverter::JsonObjectToUStruct((*Properties).ToSharedRef(), StepClass, Step, 0, CPF_Transient | CPF_DuplicateTransient))
		{
			return {};
		}
		Step->StepGUID = bPreserveGUIDs ? GUID : FGuid::NewGuid().ToString();
		if(!FGuid::Parse(Step->StepGUID, DestinationGUID) || DestinationIDs.Contains(DestinationGUID) || !FMath::IsFinite(Step->GraphPosition.X) || !FMath::IsFinite(Step->GraphPosition.Y))
		{
			return {};
		}
		DestinationIDs.Add(DestinationGUID);
		GUIDMap.Add(GUID, Step->StepGUID);
		Center += Step->GraphPosition;
		Staging->Steps.Add(Step);
	}
	Center /= Staging->Steps.Num();
	for(const TSharedPtr<FJsonValue>& Value : *SourceLinks)
	{
		const TSharedPtr<FJsonObject>* LinkObject = nullptr;
		FString ParentGUID;
		FString ChildGUID;
		if(!Value.IsValid() || !Value->TryGetObject(LinkObject) || !(*LinkObject)->TryGetStringField(TEXT("ParentGUID"), ParentGUID) || !(*LinkObject)->TryGetStringField(TEXT("ChildGUID"), ChildGUID) || !GUIDMap.Contains(ParentGUID) || !GUIDMap.Contains(ChildGUID))
		{
			return {};
		}
		FStepGraphLink Link;
		Link.ParentGUID = GUIDMap.FindChecked(ParentGUID);
		Link.ChildGUID = GUIDMap.FindChecked(ChildGUID);
		Staging->Links.Add(Link);
	}
	if(bConnectRoots)
	{
		for(const TSharedPtr<FJsonValue>& Value : *SourceRoots)
		{
			FString GUID;
			if(!Value.IsValid() || !Value->TryGetString(GUID) || !GUIDMap.Contains(GUID))
			{
				return {};
			}
			FStepGraphLink Link;
			Link.ParentGUID = Staging->StartGUID;
			Link.ChildGUID = GUIDMap.FindChecked(GUID);
			Staging->Links.Add(Link);
		}
	}
	TArray<FText> Errors;
	FStepCompiledGraph ExistingGraph;
	if(!Staging->Compile(Errors) || !Asset->BuildCompiledGraph(ExistingGraph, Errors) || Asset->Steps.Num() + Staging->Steps.Num() > 4096 || (bConnectRoots && Staging->CompiledGraph.Nodes.Num() != Staging->Steps.Num()))
	{
		return {};
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "PasteTransaction", "Import Steps"));
	ModifyForEdit(Asset);
	TArray<UStepBase*> Added;
	for(UStepBase* Staged : Staging->Steps)
	{
		UStepBase* Step = DuplicateObject<UStepBase>(Staged, Asset, MakeUniqueObjectName(Asset, Staged->GetClass()));
		Step->SetFlags(RF_Transactional);
		StepGraphOperationsPrivate::RecordObjectTree(Step);
		if(!bPreserveGUIDs)
		{
			Step->GraphPosition += Position - Center;
		}
		Step->Runtime = nullptr;
		Step->RootStep = nullptr;
		Step->ParentStep = nullptr;
		Step->SubSteps.Reset();
		Asset->Steps.Add(Step);
		Added.Add(Step);
		Step->OnGenerate();
	}
	for(FStepGraphLink Link : Staging->Links)
	{
		if(Link.ParentGUID == Staging->StartGUID)
		{
			Link.ParentGUID = Asset->StartGUID;
		}
		Asset->Links.Add(Link);
	}
	Compile(Asset);
	return Added;
}

void FStepGraphOperations::AutoLayout(UStepAsset* Asset)
{
	if(!CanEdit(Asset) || Asset->Steps.IsEmpty())
	{
		return;
	}
	FStepCompiledGraph Graph;
	TArray<FText> Errors;
	if(!Asset->BuildCompiledGraph(Graph, Errors))
	{
		return;
	}
	TMap<FString, FVector2D> Positions;
	int32 Row = 0;
	for(const FStepCompiledNode& Node : Graph.Nodes)
	{
		Positions.Add(Node.StepGUID, FVector2D((Node.Hierarchy + 1) * 340.f, Row++ * 150.f));
	}
	for(const UStepBase* Step : Asset->Steps)
	{
		if(!Positions.Contains(Step->StepGUID))
		{
			Positions.Add(Step->StepGUID, FVector2D(340.f, Row++ * 150.f));
		}
	}
	const bool bChanged = Asset->StartPosition != FVector2D::ZeroVector || Asset->Steps.ContainsByPredicate([&Positions](const UStepBase* Step){ return Step->GraphPosition != Positions.FindChecked(Step->StepGUID); });
	if(!bChanged)
	{
		return;
	}
	const FScopedTransaction Transaction(NSLOCTEXT("StepGraph", "LayoutTransaction", "Layout Steps"));
	ModifyForEdit(Asset);
	Asset->StartPosition = FVector2D::ZeroVector;
	for(UStepBase* Step : Asset->Steps)
	{
		Step->Modify();
		Step->GraphPosition = Positions.FindChecked(Step->StepGUID);
	}
	Compile(Asset);
}

void FStepGraphOperations::ModifyForEdit(UStepAsset* Asset)
{
	Asset->Modify();
	ForEachObjectWithOuter(Asset, [](UObject* Object)
	{
		Object->SetFlags(RF_Transactional);
		Object->Modify();
	}, true);
	for(TObjectIterator<UStepAssetGraph> Iterator; Iterator; ++Iterator)
	{
		UStepAssetGraph* Graph = *Iterator;
		if(Graph->StepAsset == Asset)
		{
			Graph->Modify();
			for(UEdGraphNode* Node : Graph->Nodes)
			{
				Node->Modify();
			}
		}
	}
}
