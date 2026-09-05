#include "Dialogue/DialogueModuleStatics.h"

#include "Dialogue/DialogueModule.h"

bool UDialogueModuleStatics::StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC)
{
	return UDialogueModule::IsValid() && UDialogueModule::Get().StartDialogue(InDialogue, InPlayer, InNPC);
}
bool UDialogueModuleStatics::SelectDialogueNode(int32 InNodeID)
{
	return UDialogueModule::IsValid() && UDialogueModule::Get().SelectDialogueNode(InNodeID);
}
void UDialogueModuleStatics::EndDialogue()
{
	if (UDialogueModule::IsValid()) UDialogueModule::Get().EndDialogue();
}
bool UDialogueModuleStatics::IsDialogueActive()
{
	return UDialogueModule::IsValid() && UDialogueModule::Get().IsDialogueActive();
}
UDialogueAsset* UDialogueModuleStatics::GetCurrentDialogue()
{
	return UDialogueModule::IsValid() ? UDialogueModule::Get().GetCurrentDialogue() : nullptr;
}
FDialogueNode UDialogueModuleStatics::GetCurrentDialogueNode()
{
	return UDialogueModule::IsValid() ? UDialogueModule::Get().GetCurrentNode() : FDialogueNode();
}
TArray<FDialogueNode> UDialogueModuleStatics::GetAvailableDialogueNodes()
{
	return UDialogueModule::IsValid() ? UDialogueModule::Get().GetAvailableNodes() : TArray<FDialogueNode>();
}
