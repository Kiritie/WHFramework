#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BlackboardData.h"
#include "AIBlackboardBase.generated.h"

class AAIControllerBase;
class UAIBlackboardContext;
class UBlackboardComponent;

UCLASS()
class WHFRAMEWORK_API UAIBlackboardBase : public UBlackboardData
{
	GENERATED_BODY()

public:
	UAIBlackboardBase();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<UAIBlackboardContext> ContextClass;

	virtual UAIBlackboardContext* CreateContext(AAIControllerBase* InController, UBlackboardComponent* InComponent) const;
};
