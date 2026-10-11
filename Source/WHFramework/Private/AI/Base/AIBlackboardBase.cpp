#include "AI/Base/AIBlackboardBase.h"

#include "AI/Base/AIBlackboardContext.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"

UAIBlackboardBase::UAIBlackboardBase()
{
	ContextClass = UAIBlackboardContext::StaticClass();
	FBlackboardEntry LostEntry;
	LostEntry.EntryName = TEXT("IsLostTarget");
	LostEntry.KeyType = CreateDefaultSubobject<UBlackboardKeyType_Bool>(TEXT("LostTargetKey"));
	Keys.Add(LostEntry);
	FBlackboardEntry LocationEntry;
	LocationEntry.EntryName = TEXT("TargetLocation");
	LocationEntry.KeyType = CreateDefaultSubobject<UBlackboardKeyType_Vector>(TEXT("TargetLocationKey"));
	Keys.Add(LocationEntry);
	FBlackboardEntry TargetEntry;
	TargetEntry.EntryName = TEXT("TargetAgent");
	UBlackboardKeyType_Object* TargetType = CreateDefaultSubobject<UBlackboardKeyType_Object>(TEXT("TargetAgentKey"));
	TargetType->BaseClass = AActor::StaticClass();
	TargetEntry.KeyType = TargetType;
	Keys.Add(TargetEntry);
}

UAIBlackboardContext* UAIBlackboardBase::CreateContext(AAIControllerBase* InController, UBlackboardComponent* InComponent) const
{
	if (!InController || !InComponent || !ContextClass)
	{
		return nullptr;
	}
	UAIBlackboardContext* Context = NewObject<UAIBlackboardContext>(InController, ContextClass);
	Context->Initialize(this, InController, InComponent);
	return Context;
}
