#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Dialogue/DialogueModuleTypes.h"
#include "DialogueModuleStatics.generated.h"

class UDialogueAsset;

UCLASS()
class WHFRAMEWORK_API UDialogueModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	static bool StartDialogue(UDialogueAsset* InDialogue, APlayerController* InPlayer, AActor* InNPC);

	UFUNCTION(BlueprintCallable)
	static bool SelectDialogueNode(APlayerController* InPlayer, int32 InNodeID);

	UFUNCTION(BlueprintCallable)
	static void EndDialogue(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure)
	static bool IsDialogueActive(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure)
	static UDialogueAsset* GetCurrentDialogue(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure)
	static FDialogueNode GetCurrentDialogueNode(APlayerController* InPlayer);

	UFUNCTION(BlueprintPure)
	static TArray<FDialogueNode> GetAvailableDialogueNodes(APlayerController* InPlayer);
};
