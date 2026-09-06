// Copyright Underflow Studios 2017

#pragma once

#include "Common/Interaction/InteractionConditionBase.h"
#include "DialogueConditionBase.generated.h"

UCLASS(Blueprintable, BlueprintType, abstract, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class WHFRAMEWORK_API UDialogueConditionBase : public UInteractionConditionBase
{
	GENERATED_BODY()

public:
	UDialogueConditionBase();
	virtual bool Evaluate_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dialogue Conditions")
	bool IsConditionMet(APlayerController* ConsideringPlayer, AActor* NPCActor);

	virtual class UWorld* GetWorld() const override;
};
