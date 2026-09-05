#pragma once

#include "Common/CommonModuleTypes.h"
#include "UObject/Object.h"
#include "InteractionAction.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UInteractionAction : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent)
	bool Execute(const FInteractionContext& InContext, FText& OutReason) const;

	virtual UWorld* GetWorld() const override;
};
