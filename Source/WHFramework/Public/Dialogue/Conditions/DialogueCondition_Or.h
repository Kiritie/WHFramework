#pragma once

#include "Dialogue/Base/DialogueConditionBase.h"
#include "DialogueCondition_Or.generated.h"

UCLASS(NotBlueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class WHFRAMEWORK_API UDialogueCondition_Or : public UDialogueConditionBase
{
	GENERATED_BODY()

public:
	UDialogueCondition_Or(){ }

	UPROPERTY(Instanced, EditDefaultsOnly, Category = "Dialogue Conditions")
	TArray<UDialogueConditionBase*> OrConditions;

	virtual bool IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor) override;
};
