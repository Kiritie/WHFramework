#include "AI/Base/AIServiceBase.h"
#include "AI/Base/AIControllerBase.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "GameFramework/Pawn.h"

UAIServiceBase::UAIServiceBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCreateNodeInstance = true;
	bNotifyBecomeRelevant = true;
	bNotifyCeaseRelevant = true;
}

bool UAIServiceBase::InitService(UBehaviorTreeComponent& OwnerComp)
{
	Controller = Cast<AAIControllerBase>(OwnerComp.GetAIOwner());
	Agent = Controller.IsValid() ? Controller->GetPawn() : nullptr;
	return Agent.IsValid() && Agent->HasAuthority() && GetAgent() != nullptr;
}

void UAIServiceBase::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
	Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
}

void UAIServiceBase::OnSearchStart(FBehaviorTreeSearchData& SearchData)
{
	Super::OnSearchStart(SearchData);
}

void UAIServiceBase::OnBecomeRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Super::OnBecomeRelevant(OwnerComp, NodeMemory);
	InitService(OwnerComp);
}

void UAIServiceBase::OnCeaseRelevant(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	Controller.Reset();
	Agent.Reset();
	Super::OnCeaseRelevant(OwnerComp, NodeMemory);
}

APawn* UAIServiceBase::GetAgent(TSubclassOf<APawn> InClass) const
{
	APawn* Pawn = Agent.Get();
	return Pawn && (!InClass || Pawn->IsA(InClass)) ? Pawn : nullptr;
}
