#include "Dialogue/DialogueSession.h"
#include "Dialogue/DialogueModule.h"
#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueEventBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "GameFramework/Pawn.h"

UWorld* UDialogueSession::GetWorld() const
{
    return OwnerModule.IsValid() ? OwnerModule->GetWorld() : nullptr;
}

void UDialogueSession::Initialize(UDialogueModule& InOwner, APlayerController& InPlayer)
{
    OwnerModule = &InOwner;
    OwnerPlayer = &InPlayer;
}

bool UDialogueSession::SetPersistentPlayerId(FGuid InId)
{
    UDialogueModule* Module = OwnerModule.Get();
    return Module && Module->BindPlayerIdentity(*this, InId);
}

FDialogueResumeData UDialogueSession::CaptureResume() const
{
    if (HasNetworkBridge() || bNetworkPresentation) return {};
    FDialogueResumeData Result = SavedDialogue;
    if (CurrentDialogue) { Result.DialogueAsset = SourceDialogue; Result.NodeID = CurrentNodeID; }
    return Result;
}

void UDialogueSession::RestoreResume(const FDialogueResumeData& InData)
{
    if (!CurrentDialogue && !HasNetworkBridge()) SavedDialogue = InData;
}

void UDialogueSession::Shutdown()
{
    if (NetworkBridgeOwner.IsValid()) UnbindNetworkBridge(NetworkBridgeOwner.Get());
    EndDialogue();
    OwnerPlayer.Reset();
    OwnerModule.Reset();
}
bool UDialogueSession::HasNetworkBridge() const
{
    return NetworkBridgeOwner.IsValid() && NetworkBegin.IsBound() && NetworkSelect.IsBound() &&
        NetworkContinue.IsBound() && NetworkEnd.IsBound();
}

bool UDialogueSession::IsNetworkBridgeOwner(const UObject* InOwner) const
{
    return InOwner && NetworkBridgeOwner.Get() == InOwner && HasNetworkBridge();
}

bool UDialogueSession::BindNetworkBridge(UObject* InOwner, FDialogueNetworkBegin InBegin, FDialogueNetworkSelect InSelect,
    FDialogueNetworkContinue InContinue, FDialogueNetworkEnd InEnd)
{
    if (!::IsValid(InOwner) || !InBegin.IsBound() || !InSelect.IsBound() || !InContinue.IsBound() || !InEnd.IsBound() ||
        (NetworkBridgeOwner.IsValid() && NetworkBridgeOwner.Get() != InOwner))
    {
        return false;
    }
    if (!HasNetworkBridge())
    {
        EndDialogue();
    }
    NetworkBridgeOwner = InOwner;
    NetworkBegin = MoveTemp(InBegin);
    NetworkSelect = MoveTemp(InSelect);
    NetworkContinue = MoveTemp(InContinue);
    NetworkEnd = MoveTemp(InEnd);
    SavedDialogue = FDialogueResumeData();
    return true;
}

void UDialogueSession::UnbindNetworkBridge(UObject* InOwner)
{
    if (NetworkBridgeOwner.Get() != InOwner)
    {
        return;
    }
    ClearNetworkPresentation();
    NetworkBridgeOwner.Reset();
    NetworkBegin.Unbind();
    NetworkSelect.Unbind();
    NetworkContinue.Unbind();
    NetworkEnd.Unbind();
}

void UDialogueSession::ClearNetworkPresentation()
{

    if (!bNetworkPresentation)
    {
        return;
    }
    const bool bWasActive = CurrentDialogue != nullptr;
    CurrentDialogue = nullptr;
    SourceDialogue = nullptr;
    CurrentNodeID = INDEX_NONE;
    ConsideringPlayer = nullptr;
    NPCActor = nullptr;
    NetworkNodes.Reset();
    bNetworkPresentation = false;
    if (bWasActive)
    {
        if (UDialogueModule* Host = OwnerModule.Get()) Host->NotifyChanged(*this, true);
        if (UDialogueModule* Host = OwnerModule.Get()) Host->NotifyChanged(*this, false);
    }
}

bool UDialogueSession::ApplyNetworkPresentation(UObject* InOwner, APlayerController* InPlayer, AActor* InNPC,
    const FDialogueNetworkNode& InCurrent, const TArray<FDialogueNetworkNode>& InAvailable)
{
    if (!IsNetworkBridgeOwner(InOwner) || InPlayer != OwnerPlayer.Get() || !InPlayer || !InPlayer->IsLocalController() || !::IsValid(InNPC) ||
        InCurrent.Id < 0 || InAvailable.Num() > 32)
    {
        return false;
    }
    if (!bNetworkPresentation || !CurrentDialogue)
    {
        CurrentDialogue = NewObject<UDialogueAsset>(this);
    }
    bNetworkPresentation = true;
    SourceDialogue = nullptr;
    ConsideringPlayer = InPlayer;
    NPCActor = InNPC;
    CurrentNodeID = InCurrent.Id;
    CurrentDialogue->Data.Reset();
    NetworkNodes.Reset();
    NetworkNodes.Add(InCurrent.Id, InCurrent);
    FDialogueNode Current;
    Current.id = InCurrent.Id;
    Current.Text = InCurrent.Text;
    Current.isPlayer = InCurrent.bPlayer;
    for (const FDialogueNetworkNode& Node : InAvailable)
    {
        Current.Links.AddUnique(Node.Id);
        NetworkNodes.Add(Node.Id, Node);
    }
    CurrentDialogue->Data.Add(Current);
    for (const FDialogueNetworkNode& Node : InAvailable)
    {
        if (Node.Id == Current.id)
        {
            continue;
        }
        FDialogueNode Display;
        Display.id = Node.Id;
        Display.Text = Node.Text;
        Display.isPlayer = Node.bPlayer;
        CurrentDialogue->Data.Add(MoveTemp(Display));
    }
    if (UDialogueModule* Host = OwnerModule.Get()) Host->NotifyChanged(*this, false);
    return true;
}

void UDialogueSession::GetNetworkNodeDecoration(int32 InNodeId, bool& bOutConditions, bool& bOutEvents, bool& bOutLinks) const
{
    if (bNetworkPresentation)
    {
        if (const FDialogueNetworkNode* Node = NetworkNodes.Find(InNodeId))
        {
            bOutConditions = Node->bHasConditions;
            bOutEvents = Node->bHasEvents;
            bOutLinks = Node->bHasLinks;
        }
    }
}

bool UDialogueSession::IsNodeAvailable(const FDialogueNode& Node) const
{
    if (Node.id < 0)
    {
        return false;
    }
    if (bNetworkPresentation)
    {
        return NetworkNodes.Contains(Node.id);
    }
    for (UDialogueConditionBase* Condition : Node.Conditions)
    {
        if (!Condition || !Condition->IsConditionMet(ConsideringPlayer, NPCActor))
        {
            return false;
        }
    }
    return true;
}

bool UDialogueSession::StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC)
{
    if (!::IsValid(InPlayer) || InPlayer != OwnerPlayer.Get() || !InPlayer->IsLocalController()) return false;
    if (HasNetworkBridge())
    {
        return InPlayer && InPlayer->IsLocalController() && NetworkBegin.Execute(InPlayer, InNPC);
    }
    if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
    {
        return false;
    }
    if (!InDialogue || !PersistentPlayerId.IsValid() || bSelectingNode)
    {
        return false;
    }
    TArray<FText> Errors;
    if (!InDialogue->ValidateDialogue(Errors))
    {
        return false;
    }
    const FDialogueResumeData ResumeData = CurrentDialogue ? FDialogueResumeData() : SavedDialogue;
    EndDialogue();
    if (IInteractionAgentInterface* Agent = Cast<IInteractionAgentInterface>(InNPC))
    {
        Interaction = Agent->GetInteractionComponent();
        if (!Interaction->BeginInteraction(InPlayer->GetPawn(), this))
        {
            Interaction.Reset();
            return false;
        }
        Interaction->OnInteractionCancelled.AddUniqueDynamic(this, &UDialogueSession::EndDialogue);
    }
    SavedDialogue = ResumeData;
    SourceDialogue = InDialogue;
    CurrentDialogue = DuplicateObject<UDialogueAsset>(InDialogue, this);
    CurrentDialogue->SourceObject = InDialogue;
    CurrentDialogue->AssignPersistentOuter(InPlayer->GetGameInstance());
    ConsideringPlayer = InPlayer;
    NPCActor = InNPC;
    if (NPCActor)
    {
        NPCActor->OnDestroyed.AddUniqueDynamic(this, &UDialogueSession::OnParticipantDestroyed);
    }
    if (InPlayer->GetPawn())
    {
		BoundPawn = InPlayer->GetPawn();
        InPlayer->GetPawn()->OnDestroyed.AddUniqueDynamic(this, &UDialogueSession::OnParticipantDestroyed);
    }
    if (SavedDialogue.DialogueAsset.ToSoftObjectPath() == FSoftObjectPath(InDialogue) && SavedDialogue.NodeID >= 0)
    {
        const int32 ResumeNodeID = SavedDialogue.NodeID;
        SavedDialogue = FDialogueResumeData();
        if (EnterNode(ResumeNodeID, false))
        {
            return true;
        }
    }
    const FDialogueNode Start = CurrentDialogue->GetNodeById(0);
    if (IsNodeAvailable(Start))
    {
        for (int32 Link : Start.Links)
        {
            if (EnterNode(Link, true))
            {
                return true;
            }
        }
    }
    EndDialogue();
    return false;
}

bool UDialogueSession::EnterNode(int32 InNodeID, bool bRunEvents)
{
    if (!CurrentDialogue || bSelectingNode || bNetworkPresentation)
    {
        return false;
    }
    const FDialogueNode Node = CurrentDialogue->GetNodeById(InNodeID);
    if (Node.id < 0 || (bRunEvents && !IsNodeAvailable(Node)))
    {
        return false;
    }
    TGuardValue<bool> Guard(bSelectingNode, true);
    CurrentNodeID = InNodeID;
    UDialogueAsset* Dialogue = CurrentDialogue;
    if (bRunEvents)
    {
        for (UDialogueEventBase* Event : Node.Events)
        {
            if (Event)
            {
                Event->ReceiveEventTriggered(ConsideringPlayer, NPCActor);
            }
            if (CurrentDialogue != Dialogue)
            {
                return true;
            }
        }
    }
    if (Node.isPlayer && GetAvailableNodes().IsEmpty())
    {
        EndDialogue();
        return true;
    }
    if (UDialogueModule* Host = OwnerModule.Get()) Host->NotifyChanged(*this, false);
    return true;
}

bool UDialogueSession::SelectDialogueNode(int32 InNodeID)
{
    if (HasNetworkBridge())
    {
        return bNetworkPresentation && GetCurrentNode().Links.Contains(InNodeID) && NetworkSelect.Execute(InNodeID);
    }
    if (!CurrentDialogue || bSelectingNode || !GetCurrentNode().Links.Contains(InNodeID))
    {
        return false;
    }
    if (!EnterNode(InNodeID, true))
    {
        return false;
    }
    if (CurrentDialogue && GetCurrentNode().isPlayer)
    {
        for (const FDialogueNode& Next : GetAvailableNodes())
        {
            if (!Next.isPlayer)
            {
                EnterNode(Next.id, true);
                break;
            }
        }
    }
    return true;
}

FDialogueNode UDialogueSession::GetCurrentNode() const
{
    return CurrentDialogue ? CurrentDialogue->GetNodeById(CurrentNodeID) : FDialogueNode();
}

TArray<FDialogueNode> UDialogueSession::GetAvailableNodes() const
{
    TArray<FDialogueNode> Nodes;
    if (CurrentDialogue)
    {
        for (int32 Link : GetCurrentNode().Links)
        {
            const FDialogueNode Node = CurrentDialogue->GetNodeById(Link);
            if (IsNodeAvailable(Node))
            {
                Nodes.Add(Node);
            }
        }
    }
    return Nodes;
}

TArray<FDialogueNode> UDialogueSession::GetAvailableChoices() const
{
    return GetAvailableNodes().FilterByPredicate([](const FDialogueNode& Node)
    {
        return Node.isPlayer;
    });
}

bool UDialogueSession::ContinueDialogue()
{
    if (HasNetworkBridge())
    {
        return bNetworkPresentation && NetworkContinue.Execute();
    }
    if (!CurrentDialogue || bSelectingNode || !GetAvailableChoices().IsEmpty())
    {
        return false;
    }
    for (const FDialogueNode& Node : GetAvailableNodes())
    {
        if (!Node.isPlayer)
        {
            return SelectDialogueNode(Node.id);
        }
    }
    EndDialogue();
    return true;
}

void UDialogueSession::EndDialogue()
{

    if (HasNetworkBridge())
    {
        NetworkEnd.Execute();
        ClearNetworkPresentation();
        return;
    }
    if (bNetworkPresentation)
    {
        ClearNetworkPresentation();
        return;
    }
    const bool bWasActive = CurrentDialogue != nullptr;
    if (NPCActor)
    {
        NPCActor->OnDestroyed.RemoveDynamic(this, &UDialogueSession::OnParticipantDestroyed);
    }
    if (AActor* OriginalPawn = BoundPawn.Get())
    {
        OriginalPawn->OnDestroyed.RemoveDynamic(this, &UDialogueSession::OnParticipantDestroyed);
    }
	BoundPawn.Reset();
    SavedDialogue = FDialogueResumeData();
    if (CurrentDialogue)
    {
        CurrentDialogue->CleanOuter();
    }
    CurrentDialogue = nullptr;
    SourceDialogue = nullptr;
    CurrentNodeID = INDEX_NONE;
    ConsideringPlayer = nullptr;
    NPCActor = nullptr;
    const TWeakObjectPtr<UInteractionComponent> PreviousInteraction = Interaction;
    Interaction.Reset();
    if (UInteractionComponent* Component = PreviousInteraction.Get())
    {
        Component->OnInteractionCancelled.RemoveDynamic(this, &UDialogueSession::EndDialogue);
        Component->EndInteraction(this);
    }
    if (bWasActive)
    {
        if (UDialogueModule* Host = OwnerModule.Get()) Host->NotifyChanged(*this, true);
        if (UDialogueModule* Host = OwnerModule.Get()) Host->NotifyChanged(*this, false);
    }
}

void UDialogueSession::OnParticipantDestroyed(AActor* InActor)
{
    EndDialogue();
}

