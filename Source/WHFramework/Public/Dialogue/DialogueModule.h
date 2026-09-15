#pragma once

#include "Main/Base/ModuleBase.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/DialogueModuleTypes.h"
#include "DialogueModule.generated.h"

class UInteractionComponent;

UCLASS()
class WHFRAMEWORK_API UDialogueModule : public UModuleBase
{
	GENERATED_BODY()
	GENERATED_MODULE(UDialogueModule)

public:
	UDialogueModule();
	virtual ~UDialogueModule();
	virtual void OnTermination(EPhase InPhase) override;

	UFUNCTION(BlueprintCallable)
	bool StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC);

	UFUNCTION(BlueprintCallable)
	bool SelectDialogueNode(int32 InNodeID);

	UFUNCTION(BlueprintCallable)
	void EndDialogue();

	UFUNCTION(BlueprintPure)
	FDialogueNode GetCurrentNode() const;

	UFUNCTION(BlueprintPure)
	TArray<FDialogueNode> GetAvailableNodes() const;

	UFUNCTION(BlueprintPure)
	TArray<FDialogueNode> GetAvailableChoices() const;

	UFUNCTION(BlueprintCallable)
	bool ContinueDialogue();

	UFUNCTION(BlueprintPure)
	bool IsDialogueActive() const { return CurrentDialogue != nullptr; }

	UFUNCTION(BlueprintPure)
	UDialogueAsset* GetCurrentDialogue() const { return CurrentDialogue; }

	UFUNCTION(BlueprintPure)
	AActor* GetNPCActor() const { return NPCActor; }

	UFUNCTION(BlueprintPure)
	APlayerController* GetConsideringPlayer() const { return ConsideringPlayer; }

	UPROPERTY(BlueprintAssignable)
	FOnDialogueChanged OnDialogueChanged;

	UPROPERTY(BlueprintAssignable)
	FOnDialogueChanged OnDialogueEnded;

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual void UnloadData(EPhase InPhase) override;
	virtual FParameter ToData() override;

	UPROPERTY(Transient)
	UDialogueAsset* CurrentDialogue = nullptr;

	UPROPERTY(Transient)
	UDialogueAsset* SourceDialogue = nullptr;

	UPROPERTY(Transient)
	APlayerController* ConsideringPlayer = nullptr;

	UPROPERTY(Transient)
	AActor* NPCActor = nullptr;

	UPROPERTY(Transient)
	int32 CurrentNodeID = INDEX_NONE;

	UPROPERTY(Transient)
	FDialogueModuleSaveData SavedDialogue;

	bool bSelectingNode = false;
	bool IsNodeAvailable(const FDialogueNode& Node) const;
	bool EnterNode(int32 InNodeID, bool bRunEvents);

	TWeakObjectPtr<UInteractionComponent> Interaction;

	UFUNCTION()
	void OnParticipantDestroyed(AActor* InActor);
};
