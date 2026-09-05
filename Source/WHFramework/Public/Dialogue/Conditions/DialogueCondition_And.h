#pragma once

#include "Dialogue/Base/DialogueConditionBase.h"
#include "DialogueCondition_And.generated.h"

UCLASS(NotBlueprintable, BlueprintType, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class WHFRAMEWORK_API UDialogueCondition_And : public UDialogueConditionBase
{
	GENERATED_BODY()

public:
	UDialogueCondition_And(){ }

	UPROPERTY(Instanced, EditDefaultsOnly, Category = "Dialogue Conditions")
		TArray<UDialogueConditionBase*> AndConditions;

	virtual bool IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor) override;
};
