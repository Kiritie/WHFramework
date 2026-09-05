#pragma once

#include "SaveGame/SaveGameModuleTypes.h"
#include "DialogueModuleTypes.generated.h"

class UDialogueAsset;
class UDialogueEventBase;
class UDialogueConditionBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDialogueNode
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	int32 id = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	bool isPlayer = false;

	//Todo: Make this more adjustable, allow variables in text. Allow text to be colored differently, etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	TArray<int32> Links;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	FVector2D Coordinates = FVector2D::ZeroVector;

	UPROPERTY(Instanced, EditDefaultsOnly, BlueprintReadWrite, Category = "Dialogue Node")
	TArray<UDialogueEventBase*> Events;

	UPROPERTY(Instanced, EditDefaultsOnly, BlueprintReadWrite, Category = "Dialogue Node")
	TArray<UDialogueConditionBase*> Conditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	class USoundBase* Sound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue Node")
	class UDialogueWave* DialogueWave = nullptr;
};


DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDialogueChanged);

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FDialogueModuleSaveData : public FSaveData
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftObjectPtr<UDialogueAsset> DialogueAsset;

	UPROPERTY()
	int32 NodeID = INDEX_NONE;
};
