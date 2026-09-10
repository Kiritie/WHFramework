#pragma once

#include "Common/Base/WHObject.h"
#include "Common/CommonModuleTypes.h"

#include "EventManagerBase.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UEventManagerBase : public UWHObject
{
	GENERATED_BODY()

public:
	UEventManagerBase();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnInitialize")
	void K2_OnInitialize();

	UFUNCTION()
	virtual void OnInitialize();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnPreparatory")
	void K2_OnPreparatory();

	UFUNCTION()
	virtual void OnPreparatory();

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnRefresh")
	void K2_OnRefresh(float DeltaSeconds);

	UFUNCTION()
	virtual void OnRefresh(float DeltaSeconds);

	UFUNCTION(BlueprintImplementableEvent, DisplayName = "OnTermination")
	void K2_OnTermination(EPhase InPhase);

	UFUNCTION()
	virtual void OnTermination(EPhase InPhase);

protected:
	UPROPERTY(EditAnywhere)
	FName EventManagerName;

public:
	UFUNCTION(BlueprintPure)
	FName GetEventManagerName() const { return EventManagerName; }
};
