#pragma once

#include "UObject/Interface.h"
#include "TaskAgentInterface.generated.h"

class UTaskComponent;

UINTERFACE(MinimalAPI)
class UTaskAgentInterface : public UInterface
{
	GENERATED_BODY()
};

/** 可拥有、发布或接收任务实例的角色接口 */
class WHFRAMEWORK_API ITaskAgentInterface
{
	GENERATED_BODY()

public:
	virtual UTaskComponent* GetTaskComponent() const = 0;
};
