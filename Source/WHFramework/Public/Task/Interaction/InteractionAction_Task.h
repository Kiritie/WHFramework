#pragma once

#include "Common/Interaction/InteractionActionBase.h"
#include "InteractionAction_Task.generated.h"

/** 接取或交付目标角色当前优先级最高的任务 */
UCLASS(EditInlineNew)
class WHFRAMEWORK_API UInteractionAction_Task : public UInteractionActionBase
{
	GENERATED_BODY()

public:
	virtual bool Execute_Implementation(const FInteractionContext& InContext, FText& OutReason) const override;
};
