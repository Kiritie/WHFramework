// Copyright Underflow Studios 2017

#pragma once

#include "Asset/Base/AssetBase.h"
#include "Dialogue/DialogueModuleTypes.h"
#include "DialogueAsset.generated.h"

UCLASS(Blueprintable, BlueprintType)
class WHFRAMEWORK_API UDialogueAsset : public UAssetBase
{
	GENERATED_BODY()

public:

	UDialogueAsset(const FObjectInitializer& ObjectInitializer);

	UFUNCTION(BlueprintCallable, Category = Dialogue)
	FDialogueNode GetFirstNode();

	UFUNCTION(BlueprintCallable, Category = Dialogue)
	TArray<FDialogueNode> GetNextNodes(FDialogueNode Node);

	UFUNCTION(BlueprintCallable, Category = Dialogue)
	static void CallFunctionByName(UObject* Object, FString FunctionName);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Dialogue)
	bool DisplayIdleSplines = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Dialogue)
	FText Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Dialogue)
	TArray<FDialogueNode> Data;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Dialogue)
	int32 NextNodeId = 1;

	UPROPERTY(Transient)
	class UGameInstance * PersistentGameInstance;

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void AssignPersistentOuter(class UGameInstance * inGameInstance);

	UFUNCTION(BlueprintCallable, Category = "Dialogue")
	void CleanOuter();

	virtual UWorld* GetWorld() const override;

	UFUNCTION(BlueprintPure, Category = "Dialogue")
	bool ValidateDialogue(TArray<FText>& Errors) const;
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
	FDialogueNode GetNodeById(int32 id, int32 &index);
	FDialogueNode GetNodeById(int32 id);

};
