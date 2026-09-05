#pragma once

#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueAsset.h"
#include "Task/TaskModuleTypes.h"
#include "DialogueCondition_Task.generated.h"

UCLASS(EditInlineNew)
class WHFRAMEWORK_API UDialogueCondition_Task : public UDialogueConditionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTaskReference Task;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ETaskStage RequiredStage = ETaskStage::Available;

	virtual bool IsConditionMet_Implementation(APlayerController* ConsideringPlayer, AActor* NPCActor) override;
};
