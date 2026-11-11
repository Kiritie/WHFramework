#include "Dialogue/DialogueModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Dialogue/DialogueSession.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

IMPLEMENTATION_MODULE(UDialogueModule)

UDialogueModule::UDialogueModule()
{
    ModuleName = FName("DialogueModule");
    ModuleDisplayName = NSLOCTEXT("WHFramework", "DialogueModule", "Dialogue Module");
    SaveDataVersion = 2;
    SaveScope = ESaveScope::World;
    ModuleDependencies.Add(FName("TaskModule"));
    bModuleRequired = false;
}

UDialogueModule::~UDialogueModule() {}

void UDialogueModule::OnInitialize()
{
    SaveDataVersion = 2;
    Super::OnInitialize();
}
bool UDialogueModule::StartDialogue(APlayerController* Player, UDialogueAsset* Asset, AActor* NPC)
{
    UDialogueSession* Session = GetOrCreateSession(Player);
    return Session && Session->StartDialogue(Asset, Player, NPC);
}
bool UDialogueModule::SelectDialogueNode(APlayerController* Player, int32 Node)
{
    UDialogueSession* Session = FindSession(Player);
    return Session && Session->SelectDialogueNode(Node);
}
bool UDialogueModule::ContinueDialogue(APlayerController* Player)
{
    UDialogueSession* Session = FindSession(Player);
    return Session && Session->ContinueDialogue();
}
void UDialogueModule::EndDialogue(APlayerController* Player)
{
    if (UDialogueSession* Session = FindSession(Player)) Session->EndDialogue();
}
bool UDialogueModule::IsDialogueActive(APlayerController* Player) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session && Session->IsDialogueActive();
}
UDialogueAsset* UDialogueModule::GetCurrentDialogue(APlayerController* Player) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session ? Session->GetCurrentDialogue() : nullptr;
}
FDialogueNode UDialogueModule::GetCurrentNode(APlayerController* Player) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session ? Session->GetCurrentNode() : FDialogueNode();
}
TArray<FDialogueNode> UDialogueModule::GetAvailableNodes(APlayerController* Player) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session ? Session->GetAvailableNodes() : TArray<FDialogueNode>();
}
TArray<FDialogueNode> UDialogueModule::GetAvailableChoices(APlayerController* Player) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session ? Session->GetAvailableChoices() : TArray<FDialogueNode>();
}
AActor* UDialogueModule::GetNPCActor(APlayerController* Player) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session ? Session->GetNPCActor() : nullptr;
}
bool UDialogueModule::SetPersistentPlayerId(APlayerController* Player, FGuid Id)
{
    UDialogueSession* Session = GetOrCreateSession(Player);
    return Session && BindPlayerIdentity(*Session, Id);
}
bool UDialogueModule::BindNetworkBridge(APlayerController* Player, UObject* Owner, FDialogueNetworkBegin Begin,
    FDialogueNetworkSelect Select, FDialogueNetworkContinue Continue, FDialogueNetworkEnd End)
{
    UDialogueSession* Session = GetOrCreateSession(Player);
    return Session && Session->BindNetworkBridge(Owner, MoveTemp(Begin), MoveTemp(Select), MoveTemp(Continue), MoveTemp(End));
}
bool UDialogueModule::IsNetworkBridgeOwner(APlayerController* Player, const UObject* Owner) const
{
    const UDialogueSession* Session = FindSession(Player);
    return Session && Session->IsNetworkBridgeOwner(Owner);
}
void UDialogueModule::UnbindNetworkBridge(APlayerController* Player, UObject* Owner)
{
    if (UDialogueSession* Session = FindSession(Player)) Session->UnbindNetworkBridge(Owner);
}
void UDialogueModule::ClearNetworkPresentation(APlayerController* Player, UObject* Owner)
{
    if (UDialogueSession* Session = FindSession(Player); Session && Session->IsNetworkBridgeOwner(Owner)) Session->ClearNetworkPresentation();
}
bool UDialogueModule::ApplyNetworkPresentation(APlayerController* Player, UObject* Owner, AActor* NPC,
    const FDialogueNetworkNode& Current, const TArray<FDialogueNetworkNode>& Available)
{
    UDialogueSession* Session = FindSession(Player);
    return Session && Session->ApplyNetworkPresentation(Owner, Player, NPC, Current, Available);
}
void UDialogueModule::GetNetworkNodeDecoration(APlayerController* Player, int32 Node, bool& Conditions, bool& Events, bool& Links) const
{
    Conditions = Events = Links = false;
    if (const UDialogueSession* Session = FindSession(Player)) Session->GetNetworkNodeDecoration(Node, Conditions, Events, Links);
}
void UDialogueModule::NotifyChanged(UDialogueSession& Session, bool bEnded)
{
    if (APlayerController* Player = Session.GetOwnerPlayer())
    {
        if (bEnded) OnDialogueEnded.Broadcast(Player);
        else OnDialogueChanged.Broadcast(Player);
    }
}

UDialogueSession* UDialogueModule::FindSession(APlayerController* InPlayer) const
{
    if (bShuttingDown || !::IsValid(InPlayer) || !InPlayer->IsLocalController()) return nullptr;
    for (UDialogueSession* Session : Sessions)
        if (::IsValid(Session) && Session->GetOwnerPlayer() == InPlayer) return Session;
    return nullptr;
}

UDialogueSession* UDialogueModule::GetOrCreateSession(APlayerController* InPlayer)
{
    if (bShuttingDown || !::IsValid(InPlayer) || !InPlayer->IsLocalController()) return nullptr;
    if (UDialogueSession* Existing = FindSession(InPlayer)) return Existing;
    UDialogueSession* Session = NewObject<UDialogueSession>(this);
    Session->Initialize(*this, *InPlayer);
    Sessions.Add(Session);
    return Session;
}

bool UDialogueModule::BindPlayerIdentity(UDialogueSession& InSession, FGuid InPlayerId)
{
    if (!InPlayerId.IsValid() || !Sessions.Contains(&InSession) || !InSession.GetOwnerPlayer()) return false;
    for (UDialogueSession* Other : Sessions)
        if (Other && Other != &InSession && Other->GetPersistentPlayerId() == InPlayerId) return false;
    if (InSession.PersistentPlayerId != InPlayerId)
    {

        if (InSession.PersistentPlayerId.IsValid())
        {
            const FDialogueResumeData Previous = InSession.CaptureResume();
            if (!Previous.DialogueAsset.IsNull() && Previous.NodeID >= 0)
                SavedDialogue.PlayerSessions.Add(InSession.PersistentPlayerId, Previous);
            if (InSession.HasNetworkBridge()) InSession.ClearNetworkPresentation();
            else InSession.EndDialogue();
            if (!Sessions.Contains(&InSession) || bShuttingDown) return false;
        }
        InSession.PersistentPlayerId = InPlayerId;
        if (!InSession.HasNetworkBridge()) if (FDialogueResumeData* Resume = SavedDialogue.PlayerSessions.Find(InPlayerId))
        {
            InSession.RestoreResume(*Resume);
            SavedDialogue.PlayerSessions.Remove(InPlayerId);
        }
    }
    return true;
}

void UDialogueModule::ReleaseSession(APlayerController* InPlayer)
{
    UDialogueSession* Session = FindSession(InPlayer);
    if (!Session) return;
    const FGuid Id = Session->GetPersistentPlayerId();
    const FDialogueResumeData Resume = Session->CaptureResume();
    if (Id.IsValid() && !Resume.DialogueAsset.IsNull() && Resume.NodeID >= 0) SavedDialogue.PlayerSessions.Add(Id, Resume);

    Sessions.Remove(Session);
    Session->Shutdown();
}

void UDialogueModule::OnControlRefresh(float RealDeltaSeconds)
{
    if (bShuttingDown) return;
    TArray<TWeakObjectPtr<UDialogueSession>> Retiring;
    for (int32 Index = Sessions.Num() - 1; Index >= 0; --Index)
    {
        UDialogueSession* Session = Sessions[Index];
        APlayerController* Player = Session ? Session->GetOwnerPlayer() : nullptr;
        if (!::IsValid(Player) || !Player->IsLocalController())
        {
            if (Session)
            {
                const FGuid Id = Session->GetPersistentPlayerId();
                const FDialogueResumeData Resume = Session->CaptureResume();
                if (Id.IsValid() && !Resume.DialogueAsset.IsNull() && Resume.NodeID >= 0) SavedDialogue.PlayerSessions.Add(Id, Resume);
            }
            if (Session) Retiring.Emplace(Session);
            Sessions.RemoveAt(Index);
        }
    }
    for (const TWeakObjectPtr<UDialogueSession>& Session : Retiring) if (Session.IsValid()) Session->Shutdown();
}

void UDialogueModule::OnTermination(EPhase InPhase)
{
    if (PHASEC(InPhase, EPhase::Final))
    {
		bShuttingDown = true;
        TArray<TWeakObjectPtr<UDialogueSession>> Previous;
        for (UDialogueSession* Session : Sessions) if (Session) Previous.Emplace(Session);
        Sessions.Reset();
        for (const TWeakObjectPtr<UDialogueSession>& Session : Previous) if (Session.IsValid()) Session->Shutdown();
        SavedDialogue = {};
    }
    Super::OnTermination(InPhase);
}

void UDialogueModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
    if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone || !PHASEC(InPhase, EPhase::Primary)) return;
    const FDialogueModuleSaveData& Data = InSaveData.GetRef<FDialogueModuleSaveData>();
    if (!Data.IsSaved()) return;
    SavedDialogue = Data;

    TArray<TWeakObjectPtr<UDialogueSession>> Previous;
    for (UDialogueSession* Session : Sessions) if (Session) Previous.Emplace(Session);
    for (const TWeakObjectPtr<UDialogueSession>& Entry : Previous)
    {
        UDialogueSession* Session = Entry.Get();
        if (!Session || !Sessions.Contains(Session)) continue;
        Session->EndDialogue();
        if (!Sessions.Contains(Session) || bShuttingDown) continue;
        if (FDialogueResumeData* Resume = SavedDialogue.PlayerSessions.Find(Session->GetPersistentPlayerId()))
        {
            Session->RestoreResume(*Resume);
            SavedDialogue.PlayerSessions.Remove(Session->GetPersistentPlayerId());
        }
    }
}

void UDialogueModule::UnloadData(EPhase InPhase)
{
    if (!PHASEC(InPhase, EPhase::Primary)) return;
    SavedDialogue = {};

    TArray<TWeakObjectPtr<UDialogueSession>> Previous;
    for (UDialogueSession* Session : Sessions) if (Session) Previous.Emplace(Session);
    for (const TWeakObjectPtr<UDialogueSession>& Session : Previous)
        if (Session.IsValid() && Sessions.Contains(Session.Get())) Session->EndDialogue();
}

FParameter UDialogueModule::ToData()
{
    FDialogueModuleSaveData Result = SavedDialogue;
    if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone) return FParameter(FDialogueModuleSaveData());
    for (const UDialogueSession* Session : Sessions)
    {
        if (!Session) continue;
        const FDialogueResumeData Resume = Session->CaptureResume();
        if (!Resume.DialogueAsset.IsNull() && Resume.NodeID >= 0)
        {
            if (Session->GetPersistentPlayerId().IsValid()) Result.PlayerSessions.Add(Session->GetPersistentPlayerId(), Resume);
            else UE_LOG(LogTemp, Error, TEXT("Active dialogue lacks a stable player identity and cannot be persisted."));
        }
    }
    return FParameter(MoveTemp(Result));
}
