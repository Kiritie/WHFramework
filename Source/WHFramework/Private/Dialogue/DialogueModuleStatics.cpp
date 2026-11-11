#include "Dialogue/DialogueModuleStatics.h"
#include "Dialogue/DialogueModule.h"
#include "GameFramework/PlayerController.h"

namespace
{
    UDialogueModule* FindDialogueModule(APlayerController* Player)
    {
        return ::IsValid(Player) ? UDialogueModule::GetPtr(Player->GetWorld()) : nullptr;
    }
}

bool UDialogueModuleStatics::StartDialogue(UDialogueAsset* Dialogue, APlayerController* Player, AActor* NPC)
{
    UDialogueModule* Module = FindDialogueModule(Player);
    return Module && Module->StartDialogue(Player, Dialogue, NPC);
}
bool UDialogueModuleStatics::SelectDialogueNode(APlayerController* Player, int32 Node)
{
    UDialogueModule* Module = FindDialogueModule(Player);
    return Module && Module->SelectDialogueNode(Player, Node);
}
void UDialogueModuleStatics::EndDialogue(APlayerController* Player)
{
    if (UDialogueModule* Module = FindDialogueModule(Player)) Module->EndDialogue(Player);
}
bool UDialogueModuleStatics::IsDialogueActive(APlayerController* Player)
{
    const UDialogueModule* Module = FindDialogueModule(Player);
    return Module && Module->IsDialogueActive(Player);
}
UDialogueAsset* UDialogueModuleStatics::GetCurrentDialogue(APlayerController* Player)
{
    const UDialogueModule* Module = FindDialogueModule(Player);
    return Module ? Module->GetCurrentDialogue(Player) : nullptr;
}
FDialogueNode UDialogueModuleStatics::GetCurrentDialogueNode(APlayerController* Player)
{
    const UDialogueModule* Module = FindDialogueModule(Player);
    return Module ? Module->GetCurrentNode(Player) : FDialogueNode();
}
TArray<FDialogueNode> UDialogueModuleStatics::GetAvailableDialogueNodes(APlayerController* Player)
{
    const UDialogueModule* Module = FindDialogueModule(Player);
    return Module ? Module->GetAvailableNodes(Player) : TArray<FDialogueNode>();
}
