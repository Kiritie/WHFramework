// Copyright Underflow Studios 2017

#pragma once

#include "Common/Interaction/InteractionActionBase.h"
#include "DialogueEventBase.generated.h"

UCLASS(Blueprintable, BlueprintType, abstract, EditInlineNew, HideCategories = ("DoNotShow"), CollapseCategories, AutoExpandCategories = ("Default"))
class WHFRAMEWORK_API UDialogueEventBase : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	UDialogueEventBase();
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Dialogue Events")
	void RecieveEventTriggered(APlayerController* ConsideringPlayer, AActor* NPCActor);

	virtual class UWorld* GetWorld() const override;
};
