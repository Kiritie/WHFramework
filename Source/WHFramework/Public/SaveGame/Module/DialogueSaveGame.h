#pragma once

#include "SaveGame/Module/ModuleSaveGame.h"
#include "Dialogue/DialogueModuleTypes.h"
#include "DialogueSaveGame.generated.h"

UCLASS()
class WHFRAMEWORK_API UDialogueSaveGame : public UModuleSaveGame
{
	GENERATED_BODY()

public:
	UDialogueSaveGame();

	virtual FSaveData* GetSaveData() override { return &SaveData; }
	virtual void SetSaveData(FSaveData* InSaveData) override { SaveData = InSaveData->CastRef<FDialogueModuleSaveData>(); }
	virtual FSaveData* GetDefaultData() override { return &DefaultData; }
	virtual void SetDefaultData(FSaveData* InDefaultData) override { DefaultData = InDefaultData->CastRef<FDialogueModuleSaveData>(); }

protected:
	UPROPERTY()
	FDialogueModuleSaveData SaveData;

	UPROPERTY()
	FDialogueModuleSaveData DefaultData;
};
