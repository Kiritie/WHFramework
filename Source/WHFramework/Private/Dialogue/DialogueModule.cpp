#include "Dialogue/DialogueModule.h"

#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueEventBase.h"

#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
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

bool UDialogueModule::IsNodeAvailable(const FDialogueNode& Node) const
{
	if (Node.id < 0) return false;
	for (UDialogueConditionBase* Condition : Node.Conditions)
	{
		if (!Condition || !Condition->IsConditionMet(ConsideringPlayer, NPCActor)) return false;
	}
	return true;
}

bool UDialogueModule::StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC)
{
	if (!InDialogue || !InPlayer || bSelectingNode) return false;
	TArray<FText> Errors;
	if (!InDialogue->ValidateDialogue(Errors)) return false;
	const FDialogueModuleSaveData ResumeData = CurrentDialogue ? FDialogueModuleSaveData() : SavedDialogue;
	EndDialogue();
	if (IInteractionAgentInterface* Agent = Cast<IInteractionAgentInterface>(InNPC))
	{
		Interaction = Agent->GetInteractionComponent();
		if (!Interaction->BeginInteraction(InPlayer->GetPawn(), this)) { Interaction.Reset(); return false; }
		Interaction->OnInteractionCancelled.AddUniqueDynamic(this, &UDialogueModule::EndDialogue);
	}
	SavedDialogue = ResumeData;
	SourceDialogue = InDialogue;
	CurrentDialogue = DuplicateObject<UDialogueAsset>(InDialogue, this);
	CurrentDialogue->SourceObject = InDialogue;
	CurrentDialogue->AssignPersistentOuter(InPlayer->GetGameInstance());
	ConsideringPlayer = InPlayer;
	NPCActor = InNPC;
	if (NPCActor) NPCActor->OnDestroyed.AddUniqueDynamic(this, &UDialogueModule::OnParticipantDestroyed);
	if (InPlayer->GetPawn()) InPlayer->GetPawn()->OnDestroyed.AddUniqueDynamic(this, &UDialogueModule::OnParticipantDestroyed);
	if (SavedDialogue.DialogueAsset.ToSoftObjectPath() == FSoftObjectPath(InDialogue) && SavedDialogue.NodeID > 0)
	{
		const int32 ResumeNodeID = SavedDialogue.NodeID;
		SavedDialogue = FDialogueModuleSaveData();
		if (EnterNode(ResumeNodeID, false)) return true;
	}
	const FDialogueNode Start = CurrentDialogue->GetNodeById(0);
	if (IsNodeAvailable(Start))
	{
		for (int32 Link : Start.Links)
		{
			if (EnterNode(Link, true)) return true;
		}
	}
	EndDialogue();
	return false;
}

bool UDialogueModule::EnterNode(int32 InNodeID, bool bRunEvents)
{
	if (!CurrentDialogue || bSelectingNode) return false;
	const FDialogueNode Node = CurrentDialogue->GetNodeById(InNodeID);
	if (Node.id < 0 || (bRunEvents && !IsNodeAvailable(Node))) return false;
	TGuardValue<bool> Guard(bSelectingNode, true);
	CurrentNodeID = InNodeID;
	UDialogueAsset* Dialogue = CurrentDialogue;
	if (bRunEvents)
	{
		for (UDialogueEventBase* Event : Node.Events)
		{
			if (Event) Event->ReceiveEventTriggered(ConsideringPlayer, NPCActor);
			if (CurrentDialogue != Dialogue) return true;
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
	if (!CurrentDialogue || bSelectingNode || !GetCurrentNode().Links.Contains(InNodeID)) return false;
	if (!EnterNode(InNodeID, true)) return false;
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
			if (IsNodeAvailable(Node)) Nodes.Add(Node);
		}
	}
	return Nodes;
}

TArray<FDialogueNode> UDialogueModule::GetAvailableChoices() const
{
	return GetAvailableNodes().FilterByPredicate([](const FDialogueNode& Node) { return Node.isPlayer; });
}

bool UDialogueModule::ContinueDialogue()
{
	if (!CurrentDialogue || bSelectingNode || !GetAvailableChoices().IsEmpty()) return false;
	for (const FDialogueNode& Node : GetAvailableNodes())
	{
		if (!Node.isPlayer) return SelectDialogueNode(Node.id);
	}
	EndDialogue();
	return true;
}

void UDialogueModule::EndDialogue()
{
	const bool bWasActive = CurrentDialogue != nullptr;
	if (NPCActor) NPCActor->OnDestroyed.RemoveDynamic(this, &UDialogueModule::OnParticipantDestroyed);
	if (ConsideringPlayer && ConsideringPlayer->GetPawn()) ConsideringPlayer->GetPawn()->OnDestroyed.RemoveDynamic(this, &UDialogueModule::OnParticipantDestroyed);
	SavedDialogue = FDialogueModuleSaveData();
	if (CurrentDialogue) CurrentDialogue->CleanOuter();
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
	if (PHASEC(InPhase, EPhase::Final)) EndDialogue();
}

void UDialogueModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
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
	if (CurrentDialogue)
	{
		SavedDialogue.DialogueAsset = SourceDialogue;
		SavedDialogue.NodeID = CurrentNodeID;
	}
	return FParameter(MoveTemp(SavedDialogue));
}
