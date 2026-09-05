#include "SaveGame/Module/DialogueSaveGame.h"

#include "Dialogue/DialogueModule.h"

UDialogueSaveGame::UDialogueSaveGame()
{
	SaveName = FName("Dialogue");
	ModuleClass = UDialogueModule::StaticClass();
}
