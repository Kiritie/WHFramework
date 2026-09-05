#pragma once

#include "Commandlets/Commandlet.h"
#include "DialogueSnapshotCommandlet.generated.h"

/** 导出对话定义及其内嵌条件、事件，供模块迁移核对。 */
UCLASS()
class UDialogueSnapshotCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UDialogueSnapshotCommandlet();
	virtual int32 Main(const FString& Params) override;
};
