#pragma once

#include "Common/Interaction/InteractionCondition.h"
#include "Common/Interaction/InteractionAction.h"
#include "InteractionOption.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UInteractionOption : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName OptionID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionCondition*> VisibilityConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionCondition*> EnableConditions;

	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<UInteractionAction*> Actions;

	bool IsVisible(const FInteractionContext& InContext) const;
	bool IsEnabled(const FInteractionContext& InContext, FText& OutReason) const;
	bool Execute(const FInteractionContext& InContext, FText& OutReason) const;
};
