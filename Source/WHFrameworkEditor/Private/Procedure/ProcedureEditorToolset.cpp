#include "Procedure/ProcedureEditorToolset.h"

#include "Dom/JsonObject.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Procedure/Graph/ProcedureAssetGraphSchema.h"
#include "Procedure/Graph/ProcedureGraphOperations.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UObjectIterator.h"

FString UProcedureEditorToolset::GetGraphData(UProcedureAsset* Asset)
{
	if (!Asset)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("A procedure asset is required."));
		return FString();
	}
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("asset"), Asset->GetPathName());
	Data->SetStringField(TEXT("start"), Asset->StartId.ToString());
	Data->SetStringField(TEXT("entry"), Asset->EntryNodeId.ToString());
	Data->SetBoolField(TEXT("compiled"), Asset->bCompiled);
	Data->SetStringField(TEXT("error"), Asset->CompileError);
	TArray<TSharedPtr<FJsonValue>> Nodes;
	for (const UProcedureBase* Node : Asset->Nodes)
	{
		if (Node)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(TEXT("id"), Node->NodeId.ToString());
			Item->SetStringField(TEXT("object"), Node->GetPathName());
			Item->SetStringField(TEXT("class"), Node->GetClass()->GetPathName());
			Item->SetNumberField(TEXT("x"), Node->GraphPosition.X);
			Item->SetNumberField(TEXT("y"), Node->GraphPosition.Y);
			Nodes.Add(MakeShared<FJsonValueObject>(Item));
		}
	}
	Data->SetArrayField(TEXT("nodes"), Nodes);
	TArray<TSharedPtr<FJsonValue>> Links;
	for (const FProcedureLink& Link : Asset->Links)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("from"), Link.From.ToString());
		Item->SetStringField(TEXT("output"), Link.Output.ToString());
		Item->SetStringField(TEXT("to"), Link.To.ToString());
		Links.Add(MakeShared<FJsonValueObject>(Item));
	}
	Data->SetArrayField(TEXT("links"), Links);
	TArray<TSharedPtr<FJsonValue>> Compiled;
	for (const UProcedureBase* Node : Asset->CompiledNodes)
	{
		if (Node)
		{
			Compiled.Add(MakeShared<FJsonValueString>(Node->NodeId.ToString()));
		}
	}
	Data->SetArrayField(TEXT("compiled_nodes"), Compiled);
	FString Result;
	FJsonSerializer::Serialize(Data, TJsonWriterFactory<>::Create(&Result));
	return Result;
}

TStrongObjectPtr<UProcedureAssetGraph> UProcedureEditorToolset::GetEditableGraph(UProcedureAsset* Asset)
{
	if (!FProcedureGraphOperations::CanEdit(Asset))
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Only a source procedure asset outside PIE can be edited."));
		return TStrongObjectPtr<UProcedureAssetGraph>();
	}
	for (TObjectIterator<UProcedureAssetGraph> Graph; Graph; ++Graph)
	{
		if (Graph->Asset == Asset && Graph->OnChanged.IsBound())
		{
			return TStrongObjectPtr<UProcedureAssetGraph>(*Graph);
		}
	}
	TStrongObjectPtr<UProcedureAssetGraph> Graph(NewObject<UProcedureAssetGraph>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional));
	Graph->Asset = Asset;
	Graph->Schema = UProcedureAssetGraphSchema::StaticClass();
	Graph->Rebuild();
	return Graph;
}

bool UProcedureEditorToolset::ConnectNodes(UProcedureAsset* Asset, FGuid From, FName Output, FGuid To)
{
	const TStrongObjectPtr<UProcedureAssetGraph> Graph = GetEditableGraph(Asset);
	const bool bSuccess = Graph && FProcedureGraphOperations::Connect(Graph.Get(), From, Output, To);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Connection rejected: clear conflicting edges and use a valid non-cyclic Out connection."));
	}
	return bSuccess;
}

bool UProcedureEditorToolset::InitializeStart(UProcedureAsset* Asset)
{
	const TStrongObjectPtr<UProcedureAssetGraph> Graph = GetEditableGraph(Asset);
	const bool bSuccess = Graph && FProcedureGraphOperations::InitializeStart(Graph.Get());
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("只有起点身份缺失且没有连线的源流程图可以初始化起点。"));
	}
	return bSuccess;
}

bool UProcedureEditorToolset::DisconnectNodes(UProcedureAsset* Asset, FGuid NodeId, FName PinName, bool bInput, FGuid OtherId)
{
	const TStrongObjectPtr<UProcedureAssetGraph> Graph = GetEditableGraph(Asset);
	const bool bSuccess = Graph && FProcedureGraphOperations::Disconnect(Graph.Get(), NodeId, PinName, bInput, OtherId);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("The requested source connection does not exist or cannot be edited."));
	}
	return bSuccess;
}

UProcedureBase* UProcedureEditorToolset::AddNode(UProcedureAsset* Asset, TSubclassOf<UProcedureBase> NodeClass, FVector2D Position)
{
	const TStrongObjectPtr<UProcedureAssetGraph> Graph = GetEditableGraph(Asset);
	UProcedureBase* Node = Graph ? FProcedureGraphOperations::Add(Graph.Get(), NodeClass, Position) : nullptr;
	if (!Node)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("A concrete procedure class and finite graph position are required."));
	}
	return Node;
}

bool UProcedureEditorToolset::Recompile(UProcedureAsset* Asset)
{
	const TStrongObjectPtr<UProcedureAssetGraph> Graph = GetEditableGraph(Asset);
	if (!Graph)
	{
		return false;
	}
	FProcedureGraphOperations::Changed(Graph.Get());
	FString Error;
	const bool bSuccess = Asset->ValidateCompiled(Error);
	if (!bSuccess)
	{
		UKismetSystemLibrary::RaiseScriptError(Error);
	}
	return bSuccess;
}
