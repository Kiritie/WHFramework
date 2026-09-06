#include "Common/Interaction/InteractionActionExecution.h"

#include "Common/Interaction/InteractionActionBase.h"
#include "Common/Interaction/InteractionComponent.h"
#include "GameFramework/Actor.h"

bool UInteractionActionExecution::Start(UInteractionActionBase* InAction, const FInteractionContext& InContext, FGameplayTag InOptionTag, FText& OutReason)
{
	Action = InAction;
	Context = InContext;
	Context.Execution = this;
	OptionTag = InOptionTag;
	State = EInteractionActionState::Running;
	bWaitForFinish = Action->bWaitForFinish;
	if (!GetTypedOuter<UInteractionComponent>()->BeginInteraction(Context.Interactor, this))
	{
		State = EInteractionActionState::Failed;
		GetTypedOuter<UInteractionComponent>()->OnActionEnded(this);
		return false;
	}
	Context.Target->OnDestroyed.AddUniqueDynamic(this, &UInteractionActionExecution::OnParticipantDestroyed);
	Context.Interactor->OnDestroyed.AddUniqueDynamic(this, &UInteractionActionExecution::OnParticipantDestroyed);
	bStarting = true;
	const bool bSucceeded = Action->Execute(Context, OutReason);
	bStarting = false;
	if (!bSucceeded) Finish(false);
	else if (PendingState != EInteractionActionState::None) End(PendingState);
	else if (!bWaitForFinish) Finish();
	return bSucceeded && State != EInteractionActionState::Failed && State != EInteractionActionState::Cancelled;
}

void UInteractionActionExecution::Finish(bool bSucceeded)
{
	End(bSucceeded ? EInteractionActionState::Completed : EInteractionActionState::Failed);
}

void UInteractionActionExecution::Cancel()
{
	End(EInteractionActionState::Cancelled);
}

void UInteractionActionExecution::End(EInteractionActionState InState)
{
	if (State != EInteractionActionState::Running) return;
	if (bStarting) { PendingState = InState; return; }
	State = InState;
	if (Context.Target) Context.Target->OnDestroyed.RemoveDynamic(this, &UInteractionActionExecution::OnParticipantDestroyed);
	if (Context.Interactor) Context.Interactor->OnDestroyed.RemoveDynamic(this, &UInteractionActionExecution::OnParticipantDestroyed);
	Action->Finish(Context, State);
	GetTypedOuter<UInteractionComponent>()->OnActionEnded(this);
}

void UInteractionActionExecution::OnParticipantDestroyed(AActor* InActor)
{
	Cancel();
}

UWorld* UInteractionActionExecution::GetWorld() const
{
	const UInteractionComponent* Component = GetTypedOuter<UInteractionComponent>();
	return Component ? Component->GetWorld() : nullptr;
}
