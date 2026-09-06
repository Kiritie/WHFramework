#pragma once

#include "Common/Interaction/InteractionConditionBase.h"
#include "Common/Interaction/InteractionActionBase.h"
#include "InteractionOptionBase.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UInteractionOptionBase : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag OptionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionConditionBase*> VisibilityConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionConditionBase*> EnableConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionActionBase*> Actions;

	bool IsVisible(const FInteractionContext& InContext) const;
	bool IsEnabled(const FInteractionContext& InContext, FText& OutReason) const;
};
