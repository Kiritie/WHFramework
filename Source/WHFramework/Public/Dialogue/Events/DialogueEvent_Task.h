#pragma once

#include "Dialogue/Base/DialogueEventBase.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Task/TaskModuleTypes.h"
#include "DialogueEvent_Task.generated.h"

class UTaskAsset;

UENUM(BlueprintType)
enum class EDialogueTaskAction : uint8
{
	Accept,
	Execute,
	Complete,
	TurnIn,
	Track,
	CompleteAndTurnIn UMETA(DisplayName = "Complete And Turn In")
};

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UDialogueEvent_Task : public UDialogueEventBase
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTaskReference Task;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	EDialogueTaskAction Action = EDialogueTaskAction::Accept;

	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;

	virtual void ReceiveEventTriggered_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor) override;

private:
	bool ApplyTaskAction(AActor* InTarget) const;

	UPROPERTY(Transient)
	UTaskAsset* ResolvedTaskAsset = nullptr;
};
