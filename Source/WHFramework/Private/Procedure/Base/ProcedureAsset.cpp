#include "Procedure/Base/ProcedureAsset.h"

#include "Procedure/Base/ProcedureBase.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/UObjectHash.h"

UProcedureAsset::UProcedureAsset(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    NativeClass = GetClass();
}

#if WITH_EDITORONLY_DATA
void UProcedureAsset::Serialize(FStructuredArchiveRecord Record)
{
    // 流程节点含可收集文本；保存包摘要前登记文本站点序列化所需版本。
    Record.GetUnderlyingArchive().UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
    Super::Serialize(Record);
}

void UProcedureAsset::DeclareCustomVersions(FArchive& Ar, const UClass* SpecificSubclass)
{
    Super::DeclareCustomVersions(Ar, SpecificSubclass);
    Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
}
#endif

void UProcedureAsset::Initialize()
{
    Super::Initialize();
}


bool UProcedureAsset::ValidateCompiled(FString& OutError) const
{
    OutError.Reset();
    if (!bCompiled)
    {
        OutError = CompileError.IsEmpty() ? TEXT("Procedure asset has not been compiled.") : CompileError;
        return false;
    }
    if (!EntryNodeId.IsValid())
    {
        if (!CompiledNodes.IsEmpty() || !CompiledLinks.IsEmpty())
        {
            OutError = TEXT("An unconnected Start must produce an empty program.");
            return false;
        }
        return true;
    }
    TSet<FGuid> Ids;
    for (const UProcedureBase* Node : CompiledNodes)
    {
        if (!Node || Node->GetOuter() != this || Node->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
            !Node->NodeId.IsValid() || Ids.Contains(Node->NodeId))
        {
            OutError = TEXT("Compiled Procedure contains invalid or duplicate node identity.");
            return false;
        }
        Ids.Add(Node->NodeId);
    }
    if (CompiledNodes.IsEmpty() || CompiledNodes[0]->NodeId != EntryNodeId || CompiledLinks.Num() != CompiledNodes.Num() - 1)
    {
        OutError = TEXT("Compiled Procedure order does not begin at Start or contains a disconnected suffix.");
        return false;
    }
    for (int32 Index = 0; Index < CompiledLinks.Num(); ++Index)
    {
        const FProcedureLink& Link = CompiledLinks[Index];
        if (Link.From != CompiledNodes[Index]->NodeId || Link.To != CompiledNodes[Index + 1]->NodeId || Link.Output != TEXT("Out"))
        {
            OutError = TEXT("Compiled Procedure order disagrees with its explicit default connection.");
            return false;
        }
    }
    return true;
}

#if WITH_EDITOR
UProcedureBase* UProcedureAsset::FindNode(const FGuid& InNodeId) const
{
    for (UProcedureBase* Node : Nodes)
    {
        if (Node && Node->NodeId == InNodeId)
        {
            return Node;
        }
    }
    return nullptr;
}


bool UProcedureAsset::BuildCompiled(TArray<UProcedureBase*>& OutNodes, TArray<FProcedureLink>& OutLinks, FGuid& OutEntry, FString& OutError) const
{
    OutNodes.Reset();
    OutLinks.Reset();
    OutEntry.Invalidate();
    OutError.Reset();
    if (!StartId.IsValid() || !FMath::IsFinite(StartPosition.X) || !FMath::IsFinite(StartPosition.Y))
    {
        OutError = TEXT("Procedure graph requires one valid Start identity and finite coordinates.");
        return false;
    }
    TMap<FGuid, UProcedureBase*> Definitions;
    TSet<const UProcedureBase*> Objects;
    for (UProcedureBase* Node : Nodes)
    {
        if (!Node || Node->GetOuter() != this || Node->GetClass()->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) ||
            !Node->NodeId.IsValid() || Node->NodeId == StartId || Definitions.Contains(Node->NodeId) || Objects.Contains(Node) ||
            !FMath::IsFinite(Node->GraphPosition.X) || !FMath::IsFinite(Node->GraphPosition.Y))
        {
            OutError = TEXT("Procedure graph contains an invalid class, ownership, identity, or node position.");
            return false;
        }
        Definitions.Add(Node->NodeId, Node);
        Objects.Add(Node);
    }
    TMap<FGuid, const FProcedureLink*> Outgoing;
    TSet<FGuid> HasInput;
    for (const FProcedureLink& Link : Links)
    {
        if ((Link.From != StartId && !Definitions.Contains(Link.From)) || !Definitions.Contains(Link.To) ||
            Link.From == Link.To || Link.To == StartId || Link.Output != TEXT("Out"))
        {
            OutError = TEXT("Procedure connections require valid business endpoints and one Out-to-In direction; Start has no input.");
            return false;
        }
        if (Outgoing.Contains(Link.From) || HasInput.Contains(Link.To))
        {
            OutError = TEXT("Each Procedure In and Out accepts at most one connection; duplicate connections are invalid.");
            return false;
        }
        Outgoing.Add(Link.From, &Link);
        HasInput.Add(Link.To);
    }
    TSet<FGuid> Completed;
    TArray<FGuid> ComponentStarts;
    Definitions.GetKeys(ComponentStarts);
    ComponentStarts.Add(StartId);
    for (const FGuid& ComponentStart : ComponentStarts)
    {
        TSet<FGuid> Visiting;
        FGuid Cursor = ComponentStart;
        while (Cursor.IsValid() && !Completed.Contains(Cursor))
        {
            if (Visiting.Contains(Cursor))
            {
                OutError = TEXT("Procedure default connections may not contain a cycle, including disconnected components.");
                return false;
            }
            Visiting.Add(Cursor);
            const FProcedureLink* const* Next = Outgoing.Find(Cursor);
            Cursor = Next ? (*Next)->To : FGuid();
        }
        Completed.Append(Visiting);
    }
    TArray<UProcedureBase*> BuiltNodes;
    TArray<FProcedureLink> BuiltLinks;
    FGuid BuiltEntry;
    FGuid Cursor = StartId;
    while (const FProcedureLink* const* Next = Outgoing.Find(Cursor))
    {
        const FProcedureLink& Link = **Next;
        if (Cursor == StartId)
        {
            BuiltEntry = Link.To;
        }
        else
        {
            BuiltLinks.Add(Link);
        }
        BuiltNodes.Add(Definitions.FindChecked(Link.To));
        Cursor = Link.To;
    }
    OutNodes = MoveTemp(BuiltNodes);
    OutLinks = MoveTemp(BuiltLinks);
    OutEntry = BuiltEntry;
    return true;
}

bool UProcedureAsset::Recompile()
{
    TArray<UProcedureBase*> ReachableNodes;
    TArray<FProcedureLink> ReachableLinks;
    FGuid Entry;
    FString Error;
    bCompiled = BuildCompiled(ReachableNodes, ReachableLinks, Entry, Error);
    CompiledNodes.Reset();
    CompiledLinks.Reset();
    EntryNodeId.Invalidate();
    CompileError = Error;
    if (!bCompiled)
    {
        return false;
    }
    for (UProcedureBase* Node : ReachableNodes)
    {
        CompiledNodes.Add(Node);
    }
    CompiledLinks = MoveTemp(ReachableLinks);
    EntryNodeId = Entry;
    return true;
}

void UProcedureAsset::PreSave(FObjectPreSaveContext SaveContext)
{
    if (!Recompile())
    {
        UE_LOG(LogTemp, Error, TEXT("Procedure compile failed for %s: %s"), *GetPathName(), *CompileError);
    }
    Super::PreSave(SaveContext);
}

void UProcedureAsset::PreEditChange(FProperty* PropertyAboutToChange)
{
    Super::PreEditChange(PropertyAboutToChange);
    if (SourceObject)
    {
        return;
    }
    SetFlags(RF_Transactional);
    Modify();
    TArray<UObject*> Objects;
    GetObjectsWithOuter(this, Objects, true);
    for (UObject* Object : Objects)
    {
        Object->SetFlags(RF_Transactional);
        Object->Modify();
    }
}

void UProcedureAsset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    Recompile();
}
#endif
