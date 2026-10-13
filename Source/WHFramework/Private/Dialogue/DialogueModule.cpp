#include "Dialogue/DialogueModule.h"

#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueEventBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Common/Interaction/InteractionComponent.h"
#include "Common/Interaction/InteractionAgentInterface.h"
#include "GameFramework/Pawn.h"

IMPLEMENTATION_MODULE(UDialogueModule)

UDialogueModule::UDialogueModule()
{
    ModuleName = FName("DialogueModule");
    ModuleDisplayName = NSLOCTEXT("WHFramework", "DialogueModule", "Dialogue Module");
    SaveScope = ESaveScope::World;
    ModuleDependencies.Add(FName("TaskModule"));
    bModuleRequired = false;
}

UDialogueModule::~UDialogueModule()
{
    TERMINATION_MODULE(UDialogueModule)
}

bool UDialogueModule::HasNetworkBridge() const
{
    return NetworkBridgeOwner.IsValid() && NetworkBegin.IsBound() && NetworkSelect.IsBound() &&
        NetworkContinue.IsBound() && NetworkEnd.IsBound();
}

bool UDialogueModule::IsNetworkBridgeOwner(const UObject* InOwner) const
{
    return InOwner && NetworkBridgeOwner.Get() == InOwner && HasNetworkBridge();
}

bool UDialogueModule::BindNetworkBridge(UObject* InOwner, FDialogueNetworkBegin InBegin, FDialogueNetworkSelect InSelect,
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
    SavedDialogue = FDialogueModuleSaveData();
    return true;
}

void UDialogueModule::UnbindNetworkBridge(UObject* InOwner)
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

void UDialogueModule::ClearNetworkPresentation()
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
        OnDialogueEnded.Broadcast();
        OnDialogueChanged.Broadcast();
    }
}

bool UDialogueModule::ApplyNetworkPresentation(UObject* InOwner, APlayerController* InPlayer, AActor* InNPC,
    const FDialogueNetworkNode& InCurrent, const TArray<FDialogueNetworkNode>& InAvailable)
{
    if (!IsNetworkBridgeOwner(InOwner) || !InPlayer || !InPlayer->IsLocalController() || !::IsValid(InNPC) ||
        InPlayer->GetWorld() != GetWorld() || InNPC->GetWorld() != GetWorld() || InCurrent.Id < 0 || InAvailable.Num() > 32)
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
    OnDialogueChanged.Broadcast();
    return true;
}

void UDialogueModule::GetNetworkNodeDecoration(int32 InNodeId, bool& bOutConditions, bool& bOutEvents, bool& bOutLinks) const
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

bool UDialogueModule::IsNodeAvailable(const FDialogueNode& Node) const
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

bool UDialogueModule::StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC)
{
    if (HasNetworkBridge())
    {
        return InPlayer && InPlayer->IsLocalController() && NetworkBegin.Execute(InPlayer, InNPC);
    }
    if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
    {
        return false;
    }
    if (!InDialogue || !InPlayer || bSelectingNode)
    {
        return false;
    }
    TArray<FText> Errors;
    if (!InDialogue->ValidateDialogue(Errors))
    {
        return false;
    }
    const FDialogueModuleSaveData ResumeData = CurrentDialogue ? FDialogueModuleSaveData() : SavedDialogue;
    EndDialogue();
    if (IInteractionAgentInterface* Agent = Cast<IInteractionAgentInterface>(InNPC))
    {
        Interaction = Agent->GetInteractionComponent();
        if (!Interaction->BeginInteraction(InPlayer->GetPawn(), this))
        {
            Interaction.Reset();
            return false;
        }
        Interaction->OnInteractionCancelled.AddUniqueDynamic(this, &UDialogueModule::EndDialogue);
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
        NPCActor->OnDestroyed.AddUniqueDynamic(this, &UDialogueModule::OnParticipantDestroyed);
    }
    if (InPlayer->GetPawn())
    {
        InPlayer->GetPawn()->OnDestroyed.AddUniqueDynamic(this, &UDialogueModule::OnParticipantDestroyed);
    }
    if (SavedDialogue.DialogueAsset.ToSoftObjectPath() == FSoftObjectPath(InDialogue) && SavedDialogue.NodeID > 0)
    {
        const int32 ResumeNodeID = SavedDialogue.NodeID;
        SavedDialogue = FDialogueModuleSaveData();
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

bool UDialogueModule::EnterNode(int32 InNodeID, bool bRunEvents)
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
    OnDialogueChanged.Broadcast();
    return true;
}

bool UDialogueModule::SelectDialogueNode(int32 InNodeID)
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

FDialogueNode UDialogueModule::GetCurrentNode() const
{
    return CurrentDialogue ? CurrentDialogue->GetNodeById(CurrentNodeID) : FDialogueNode();
}

TArray<FDialogueNode> UDialogueModule::GetAvailableNodes() const
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

TArray<FDialogueNode> UDialogueModule::GetAvailableChoices() const
{
    return GetAvailableNodes().FilterByPredicate([](const FDialogueNode& Node)
    {
        return Node.isPlayer;
    });
}

bool UDialogueModule::ContinueDialogue()
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

void UDialogueModule::EndDialogue()
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
        NPCActor->OnDestroyed.RemoveDynamic(this, &UDialogueModule::OnParticipantDestroyed);
    }
    if (ConsideringPlayer && ConsideringPlayer->GetPawn())
    {
        ConsideringPlayer->GetPawn()->OnDestroyed.RemoveDynamic(this, &UDialogueModule::OnParticipantDestroyed);
    }
    SavedDialogue = FDialogueModuleSaveData();
    if (CurrentDialogue)
    {
        CurrentDialogue->CleanOuter();
    }
    CurrentDialogue = nullptr;
    SourceDialogue = nullptr;
    CurrentNodeID = INDEX_NONE;
    ConsideringPlayer = nullptr;
    NPCActor = nullptr;
    if (Interaction.IsValid())
    {
        Interaction->OnInteractionCancelled.RemoveDynamic(this, &UDialogueModule::EndDialogue);
        Interaction->EndInteraction(this);
    }
    Interaction.Reset();
    if (bWasActive)
    {
        OnDialogueEnded.Broadcast();
        OnDialogueChanged.Broadcast();
    }
}

void UDialogueModule::OnParticipantDestroyed(AActor* InActor)
{
    EndDialogue();
}

void UDialogueModule::OnTermination(EPhase InPhase)
{
    Super::OnTermination(InPhase);
    if (PHASEC(InPhase, EPhase::Final))
    {
        EndDialogue();
    }
}

void UDialogueModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
    if (HasNetworkBridge() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
    {
        return;
    }
    if (InSaveData.GetRef<FDialogueModuleSaveData>().IsSaved() && PHASEC(InPhase, EPhase::Primary))
    {
        EndDialogue();
        SavedDialogue = InSaveData.GetRef<FDialogueModuleSaveData>();
    }
}

void UDialogueModule::UnloadData(EPhase InPhase)
{
    if (PHASEC(InPhase, EPhase::Primary))
    {
        EndDialogue();
        SavedDialogue = FDialogueModuleSaveData();
    }
}

FParameter UDialogueModule::ToData()
{
    if (HasNetworkBridge() || bNetworkPresentation || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone)
    {
        return FParameter(FDialogueModuleSaveData());
    }
    if (CurrentDialogue)
    {
        SavedDialogue.DialogueAsset = SourceDialogue;
        SavedDialogue.NodeID = CurrentNodeID;
    }
    return FParameter(SavedDialogue);
}
