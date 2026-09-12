#pragma once

#include "CommonActionWidget.h"
#include "GameplayTagContainer.h"

#include "WidgetInputAction.generated.h"

UCLASS(BlueprintType)
class WHFRAMEWORK_API UWidgetInputAction : public UCommonActionWidget
{
	GENERATED_BODY()

public:
	UWidgetInputAction(const FObjectInitializer& ObjectInitializer);

	virtual void SynchronizeProperties() override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input", meta = (Categories = "Input"))
	FGameplayTag ActionTag;

public:
	UFUNCTION(BlueprintPure, Category = "Input")
	FGameplayTag GetActionTag() const { return ActionTag; }

	UFUNCTION(BlueprintCallable, Category = "Input")
	void SetActionTag(FGameplayTag InActionTag);
};
