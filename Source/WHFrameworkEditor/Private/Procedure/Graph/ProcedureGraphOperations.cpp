#include "Procedure/Graph/ProcedureGraphOperations.h"

#include "Procedure/Base/ProcedureAsset.h"
#include "Procedure/Base/ProcedureBase.h"
#include "Procedure/Graph/ProcedureAssetGraph.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "JsonObjectConverter.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"

namespace ProcedureGraphOperationsPrivate
{
void RecordDefinition(UProcedureBase* Node)
{
    Node->SetFlags(RF_Transactional);
    Node->Modify();
    TArray<UObject*> Subobjects;
    GetObjectsWithOuter(Node, Subobjects, true);
    for (UObject* Subobject : Subobjects)
    {
        Subobject->SetFlags(RF_Transactional);
        Subobject->Modify();
    }
}
}

bool FProcedureGraphOperations::CanEdit(const UProcedureAsset* Asset)
{
    return Asset && !Asset->SourceObject && GEditor && !GEditor->PlayWorld;
}

void FProcedureGraphOperations::Record(UProcedureAssetGraph* Graph)
{
    Graph->Modify();
    Graph->Asset->Modify();
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        Node->Modify();
    }
    TArray<UObject*> Objects;
    GetObjectsWithOuter(Graph->Asset, Objects, true);
    for (UObject* Object : Objects)
    {
        Object->SetFlags(RF_Transactional);
        Object->Modify();
    }
}

void FProcedureGraphOperations::Changed(UProcedureAssetGraph* Graph)
{
    Graph->Asset->Recompile();
    Graph->Asset->MarkPackageDirty();
    Graph->OnChanged.ExecuteIfBound();
}


bool FProcedureGraphOperations::CanConnect(const UProcedureAsset* Asset, FGuid From, FName Output, FGuid To)
{
    if (!Asset || !From.IsValid() || To == Asset->StartId || !Asset->FindNode(To) || Output != TEXT("Out") ||
        (From != Asset->StartId && !Asset->FindNode(From)))
    {
        return false;
    }
    if (Asset->Links.ContainsByPredicate([&](const FProcedureLink& Link)
        { return Link.From == From && Link.Output == Output && Link.To == To; }))
    {
        return false;
    }
    if (Asset->Links.ContainsByPredicate([&](const FProcedureLink& Link)
        { return (Link.From == From && Link.To != To) || (Link.To == To && Link.From != From); }))
    {
        return false;
    }
    TSet<FGuid> Visited;
    FGuid Cursor = To;
    while (Cursor.IsValid())
    {
        if (Cursor == From || Visited.Contains(Cursor))
        {
            return false;
        }
        Visited.Add(Cursor);
        const FProcedureLink* Link = Asset->Links.FindByPredicate([&](const FProcedureLink& Edge) { return Edge.From == Cursor; });
        Cursor = Link ? Link->To : FGuid();
    }
    return true;
}

bool FProcedureGraphOperations::InitializeStart(UProcedureAssetGraph* Graph)
{
    if (!Graph || !CanEdit(Graph->Asset) || Graph->Asset->StartId.IsValid() || !Graph->Asset->Links.IsEmpty())
    {
        return false;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "InitializeStart", "初始化流程起点"));
    Record(Graph);
    Graph->Asset->StartId = FGuid::NewGuid();
    Graph->Asset->StartPosition = FVector2D::ZeroVector;
    Changed(Graph);
    return true;
}

bool FProcedureGraphOperations::Connect(UProcedureAssetGraph* Graph, FGuid From, FName Output, FGuid To)
{
    if (!Graph || !CanEdit(Graph->Asset) || !CanConnect(Graph->Asset, From, Output, To))
    {
        return false;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "Connect", "连接流程节点"));
    Record(Graph);
    Graph->Asset->Links.RemoveAll([&](const FProcedureLink& Link) { return Link.From == From || Link.To == To; });
    FProcedureLink Link;
    Link.From = From;
    Link.Output = Output;
    Link.To = To;
    Graph->Asset->Links.Add(Link);
    Changed(Graph);
    return true;
}

bool FProcedureGraphOperations::Disconnect(UProcedureAssetGraph* Graph, FGuid NodeId, FName PinName, bool bInput, FGuid OtherId)
{
    if (!Graph || !CanEdit(Graph->Asset))
    {
        return false;
    }
    const auto Matches = [&](const FProcedureLink& Link)
    {
        if (PinName.IsNone())
        {
            return Link.From == NodeId || Link.To == NodeId;
        }
        if (bInput)
        {
            return Link.To == NodeId && (!OtherId.IsValid() || Link.From == OtherId);
        }
        return Link.From == NodeId && Link.Output == PinName && (!OtherId.IsValid() || Link.To == OtherId);
    };
    if (!Graph->Asset->Links.ContainsByPredicate(Matches))
    {
        return false;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "Disconnect", "断开流程连线"));
    Record(Graph);
    Graph->Asset->Links.RemoveAll(Matches);
    Changed(Graph);
    return true;
}

UProcedureBase* FProcedureGraphOperations::Add(UProcedureAssetGraph* Graph, UClass* Class, FVector2D Position, FGuid Source, FName Output, FGuid Target)
{
    if (!Graph || !CanEdit(Graph->Asset) || !Class || !Class->IsChildOf(UProcedureBase::StaticClass()) ||
        Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
        !FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y))
    {
        return nullptr;
    }
    if ((Target.IsValid() && (!Graph->Asset->FindNode(Target))) ||
        (Source.IsValid() && (Source != Graph->Asset->StartId && !Graph->Asset->FindNode(Source))))
    {
        return nullptr;
    }
    if (Source.IsValid())
    {
        if (Output != TEXT("Out"))
        {
            return nullptr;
        }
    }
    if (Graph->Asset->Links.ContainsByPredicate([&](const FProcedureLink& Link)
        { return (Source.IsValid() && Link.From == Source) || (Target.IsValid() && Link.To == Target); }))
    {
        return nullptr;
    }
    if (Source.IsValid() && Target.IsValid())
    {
        TSet<FGuid> Visited;
        FGuid Cursor = Target;
        while (Cursor.IsValid())
        {
            if (Cursor == Source || Visited.Contains(Cursor))
            {
                return nullptr;
            }
            Visited.Add(Cursor);
            const FProcedureLink* Next = Graph->Asset->Links.FindByPredicate(
                [&](const FProcedureLink& Link) { return Link.From == Cursor; });
            Cursor = Next ? Next->To : FGuid();
        }
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "Add", "创建流程节点"));
    Record(Graph);
    UProcedureBase* Node = NewObject<UProcedureBase>(Graph->Asset, Class, NAME_None, RF_Transactional);
    Node->Modify();
    Node->NodeId = FGuid::NewGuid();
    Node->GraphPosition = Position;
    Graph->Asset->Nodes.Add(Node);
    ProcedureGraphOperationsPrivate::RecordDefinition(Node);
    Node->OnGenerate();
    if (Source.IsValid())
    {
        Graph->Asset->Links.RemoveAll([&](const FProcedureLink& Link) { return Link.From == Source && Link.Output == Output; });
        FProcedureLink Link;
        Link.From = Source;
        Link.Output = Output;
        Link.To = Node->NodeId;
        Graph->Asset->Links.Add(Link);
    }
    if (Target.IsValid())
    {
        Graph->Asset->Links.RemoveAll([&](const FProcedureLink& Existing) { return Existing.To == Target; });
        FProcedureLink Link;
        Link.From = Node->NodeId;
        Link.To = Target;
        Graph->Asset->Links.Add(Link);
    }
    Changed(Graph);
    return Node;
}

bool FProcedureGraphOperations::Delete(UProcedureAssetGraph* Graph, const TArray<FGuid>& Ids)
{
    if (!Graph || !CanEdit(Graph->Asset))
    {
        return false;
    }
    TSet<FGuid> Deleted;
    for (const FGuid& Id : Ids)
    {
        if (Id != Graph->Asset->StartId && Graph->Asset->FindNode(Id))
        {
            Deleted.Add(Id);
        }
    }
    if (Deleted.IsEmpty())
    {
        return false;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "Delete", "删除流程节点"));
    Record(Graph);
    for (UProcedureBase* Node : Graph->Asset->Nodes)
    {
        if (Deleted.Contains(Node->NodeId))
        {
            Node->OnUnGenerate();
        }
    }
    Graph->Asset->Links.RemoveAll([&](const FProcedureLink& Link) { return Deleted.Contains(Link.From) || Deleted.Contains(Link.To); });
    Graph->Asset->Nodes.RemoveAll([&](const TObjectPtr<UProcedureBase>& Node) { return Node && Deleted.Contains(Node->NodeId); });
    Changed(Graph);
    return true;
}

UProcedureBase* FProcedureGraphOperations::ChangeType(UProcedureAssetGraph* Graph, FGuid Id, UClass* Class)
{
    UProcedureBase* Previous = Graph && Graph->Asset ? Graph->Asset->FindNode(Id) : nullptr;
    if (!Graph || !CanEdit(Graph->Asset) || !Previous || !Class || Previous->GetClass() == Class ||
        !Class->IsChildOf(UProcedureBase::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
    {
        return nullptr;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "ChangeType", "修改流程节点类型"));
    Record(Graph);
    UProcedureBase* Replacement = NewObject<UProcedureBase>(Graph->Asset, Class, NAME_None, RF_Transactional);
    Replacement->Modify();
    UEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
    Params.bNotifyObjectReplacement = false;
    UEngine::CopyPropertiesForUnrelatedObjects(Previous, Replacement, Params);
    Replacement->NodeId = Id;
    Replacement->GraphPosition = Previous->GraphPosition;
    Graph->Asset->Nodes[Graph->Asset->Nodes.IndexOfByKey(Previous)] = Replacement;
    Previous->OnUnGenerate();
    ProcedureGraphOperationsPrivate::RecordDefinition(Replacement);
    Replacement->OnGenerate();
    Changed(Graph);
    return Replacement;
}

bool FProcedureGraphOperations::Export(const UProcedureAsset* Asset, const TArray<FGuid>& Ids, FString& Text)
{
    Text.Reset();
    if (!Asset)
    {
        return false;
    }
    TSet<FGuid> Selected;
    TArray<TSharedPtr<FJsonValue>> Nodes;
    for (const FGuid& Id : Ids)
    {
        UProcedureBase* Node = Asset->FindNode(Id);
        if (!Node || Selected.Contains(Id))
        {
            continue;
        }
        TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
        if (!FJsonObjectConverter::UStructToJsonObject(Node->GetClass(), Node, Data, 0, CPF_Transient, nullptr,
            EJsonObjectConversionFlags::WriteTextAsComplexString | EJsonObjectConversionFlags::SkipStandardizeCase))
        {
            return false;
        }
        Data->SetStringField(TEXT("_ClassName"), Node->GetClass()->GetPathName());
        Nodes.Add(MakeShared<FJsonValueObject>(Data));
        Selected.Add(Id);
    }
    if (Nodes.IsEmpty())
    {
        return false;
    }
    TArray<TSharedPtr<FJsonValue>> Links;
    for (const FProcedureLink& Link : Asset->Links)
    {
        if (Selected.Contains(Link.From) && Selected.Contains(Link.To))
        {
            TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
            FJsonObjectConverter::UStructToJsonObject(FProcedureLink::StaticStruct(), &Link, Data, 0, 0);
            Links.Add(MakeShared<FJsonValueObject>(Data));
        }
    }
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("Format"), TEXT("WHFramework.ProcedureGraph.1"));
    Root->SetArrayField(TEXT("Nodes"), Nodes);
    Root->SetArrayField(TEXT("Links"), Links);
    return FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text));
}

bool FProcedureGraphOperations::CanImport(const FString& Text)
{
    TSharedPtr<FJsonObject> Root;
    const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Links = nullptr;
    FString Format;
    return Text.Len() <= 8 * 1024 * 1024 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) && Root &&
        Root->TryGetStringField(TEXT("Format"), Format) && Format == TEXT("WHFramework.ProcedureGraph.1") &&
        Root->TryGetArrayField(TEXT("Nodes"), Nodes) && !Nodes->IsEmpty() && Nodes->Num() <= 4096 &&
        Root->TryGetArrayField(TEXT("Links"), Links) && Links->Num() <= 4096;
}

TArray<FGuid> FProcedureGraphOperations::Import(UProcedureAssetGraph* Graph, const FString& Text, FVector2D Position)
{
    if (!Graph || !CanEdit(Graph->Asset) || !CanImport(Text) || !FMath::IsFinite(Position.X) || !FMath::IsFinite(Position.Y))
    {
        return {};
    }
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
    {
        return {};
    }
    TStrongObjectPtr<UProcedureAsset> Temporary(NewObject<UProcedureAsset>(GetTransientPackage()));
    for (const TSharedPtr<FJsonValue>& Value : Root->GetArrayField(TEXT("Nodes")))
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            return {};
        }
        const TSharedPtr<FJsonObject> Data = Value->AsObject();
        FString ClassPath;
        if (!Data || !Data->TryGetStringField(TEXT("_ClassName"), ClassPath))
        {
            return {};
        }
        UClass* Class = LoadObject<UClass>(nullptr, *ClassPath);
        if (!Class || !Class->IsChildOf(UProcedureBase::StaticClass()) ||
            Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            return {};
        }
        UProcedureBase* Node = NewObject<UProcedureBase>(Temporary.Get(), Class);
        if (!FJsonObjectConverter::JsonObjectToUStruct(Data.ToSharedRef(), Class, Node, 0, CPF_Transient))
        {
            return {};
        }
        Temporary->Nodes.Add(Node);
    }
    const TArray<TSharedPtr<FJsonValue>>* LinkValues = nullptr;
    if (!Root->TryGetArrayField(TEXT("Links"), LinkValues))
    {
        return {};
    }
    TMap<FGuid, FGuid> Remap;
    FVector2D Center = FVector2D::ZeroVector;
    for (UProcedureBase* Node : Temporary->Nodes)
    {
        if (!Node->NodeId.IsValid() || Remap.Contains(Node->NodeId) || !FMath::IsFinite(Node->GraphPosition.X) || !FMath::IsFinite(Node->GraphPosition.Y))
        {
            return {};
        }
        Remap.Add(Node->NodeId, FGuid::NewGuid());
        Center += Node->GraphPosition;
    }
    TSet<FString> UsedOutputs;
    for (const TSharedPtr<FJsonValue>& Value : *LinkValues)
    {
        if (!Value.IsValid() || Value->Type != EJson::Object)
        {
            return {};
        }
        const TSharedPtr<FJsonObject> Data = Value->AsObject();
        FProcedureLink Link;
        if (!Data || !FJsonObjectConverter::JsonObjectToUStruct(Data.ToSharedRef(), FProcedureLink::StaticStruct(), &Link, 0, 0) ||
            !Remap.Contains(Link.From) || !Remap.Contains(Link.To))
        {
            return {};
        }
        const UProcedureBase* Source = Temporary->FindNode(Link.From);
        const FString Key = Link.From.ToString() + TEXT("/") + Link.Output.ToString();
        if (!Source || Link.Output != TEXT("Out") || UsedOutputs.Contains(Key))
        {
            return {};
        }
        UsedOutputs.Add(Key);
        Temporary->Links.Add(Link);
    }
    Temporary->StartId = FGuid::NewGuid();
    if (!Temporary->Recompile())
    {
        return {};
    }
    Center /= Temporary->Nodes.Num();
    TMap<FGuid, FVector2D> NewPositions;
    for (const UProcedureBase* Node : Temporary->Nodes)
    {
        const FVector2D NewPosition = Node->GraphPosition - Center + Position;
        if (!FMath::IsFinite(NewPosition.X) || !FMath::IsFinite(NewPosition.Y))
        {
            return {};
        }
        NewPositions.Add(Node->NodeId, NewPosition);
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "Paste", "粘贴流程节点"));
    Record(Graph);
    TArray<FGuid> Added;
    for (UProcedureBase* Source : Temporary->Nodes)
    {
        UProcedureBase* Node = DuplicateObject<UProcedureBase>(Source, Graph->Asset);
        Node->SetFlags(RF_Transactional);
        Node->ClearFlags(RF_Transient);
        Node->Modify();
        Node->NodeId = Remap.FindChecked(Source->NodeId);
        Node->GraphPosition = NewPositions.FindChecked(Source->NodeId);
        Graph->Asset->Nodes.Add(Node);
        ProcedureGraphOperationsPrivate::RecordDefinition(Node);
        Node->OnGenerate();
        Added.Add(Node->NodeId);
    }
    for (FProcedureLink Link : Temporary->Links)
    {
        Link.From = Remap.FindChecked(Link.From);
        Link.To = Remap.FindChecked(Link.To);
        Graph->Asset->Links.Add(Link);
    }
    Changed(Graph);
    return Added;
}

bool FProcedureGraphOperations::AutoLayout(UProcedureAssetGraph* Graph)
{
    if (!Graph || !CanEdit(Graph->Asset))
    {
        return false;
    }
    UProcedureAsset* Asset = Graph->Asset;
    TMap<FGuid, int32> Depths;
    TArray<FGuid> Queue;
    Depths.Add(Asset->StartId, 0);
    Queue.Add(Asset->StartId);
    for (int32 Cursor = 0; Cursor < Queue.Num(); ++Cursor)
    {
        const FGuid Source = Queue[Cursor];
        for (const FProcedureLink& Link : Asset->Links)
        {
            if (Link.From == Source && Asset->FindNode(Link.To) && !Depths.Contains(Link.To))
            {
                Depths.Add(Link.To, Depths.FindChecked(Source) + 1);
                Queue.Add(Link.To);
            }
        }
    }
    TMap<int32, int32> Rows;
    TMap<FGuid, FVector2D> Positions;
    int32 DisconnectedRow = 0;
    for (UProcedureBase* Node : Asset->Nodes)
    {
        const int32* Depth = Depths.Find(Node->NodeId);
        const FVector2D Position = Depth ? FVector2D(*Depth * 360.f, Rows.FindOrAdd(*Depth)++ * 180.f) :
            FVector2D(360.f, 600.f + DisconnectedRow++ * 180.f);
        Positions.Add(Node->NodeId, Position);
    }
    const bool bChanged = Asset->StartPosition != FVector2D::ZeroVector || Asset->Nodes.ContainsByPredicate(
        [&](const TObjectPtr<UProcedureBase>& Node) { return Node && Node->GraphPosition != Positions.FindChecked(Node->NodeId); });
    if (!bChanged)
    {
        return false;
    }
    const FScopedTransaction Transaction(NSLOCTEXT("ProcedureGraph", "Layout", "排列流程图"));
    Record(Graph);
    Asset->StartPosition = FVector2D::ZeroVector;
    for (UProcedureBase* Node : Asset->Nodes)
    {
        Node->GraphPosition = Positions.FindChecked(Node->NodeId);
    }
    Changed(Graph);
    return true;
}
