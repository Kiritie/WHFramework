#pragma once

#include "Event/Events/Common/Game/Event_GameExited.h"
#include "Event/Events/Common/Game/Event_GameInited.h"
#include "Event/Events/Common/Game/Event_GameStarted.h"
#include "EventManagerBase.h"

#include "DefaultEventManagerBase.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UDefaultEventManagerBase : public UEventManagerBase
{
	GENERATED_BODY()

public:
	UDefaultEventManagerBase();

	virtual void OnInitialize() override;
	virtual void OnPreparatory() override;
	virtual void OnRefresh(float DeltaSeconds) override;
	virtual void OnTermination(EPhase InPhase) override;

protected:
	UFUNCTION(BlueprintNativeEvent)
	void OnGameInited(UObject* InSender, const FEventGameInited& InEvent);

	UFUNCTION(BlueprintNativeEvent)
	void OnGameStarted(UObject* InSender, const FEventGameStarted& InEvent);

	UFUNCTION(BlueprintNativeEvent)
	void OnGameExited(UObject* InSender, const FEventGameExited& InEvent);
};
