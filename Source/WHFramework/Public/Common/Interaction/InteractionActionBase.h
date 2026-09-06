#pragma once

#include "Common/CommonModuleTypes.h"
#include "UObject/Object.h"
#include "InteractionActionBase.generated.h"

UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WHFRAMEWORK_API UInteractionActionBase : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bWaitForFinish = false;

	UFUNCTION(BlueprintNativeEvent)
	bool Execute(const FInteractionContext& InContext, FText& OutReason) const;

	UFUNCTION(BlueprintNativeEvent)
	void Finish(const FInteractionContext& InContext, EInteractionActionState InState) const;

	virtual UWorld* GetWorld() const override;
};
