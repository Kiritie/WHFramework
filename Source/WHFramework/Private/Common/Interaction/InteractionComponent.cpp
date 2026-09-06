// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/Interaction/InteractionComponent.h"

#include "Common/Interaction/InteractionAgentInterface.h"
#include "Common/Interaction/InteractionOptionBase.h"
#include "Common/Interaction/InteractionActionExecution.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/ScopeExit.h"

UInteractionComponent::UInteractionComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	
	UPrimitiveComponent::SetCollisionProfileName(TEXT("Interaction"));
	InitBoxExtent(FVector(100.f, 100.f, 100.f));
	OnComponentBeginOverlap.AddDynamic(this, &UInteractionComponent::OnBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UInteractionComponent::OnEndOverlap);
}

void UInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bEndingPlay = true;
	CancelInteractions();
	IInteractionAgentInterface* Agent = GetInteractionAgent();
	if (Agent)
	{
		if (IInteractionAgentInterface* Other = Agent->GetInteractingAgent(); Other && Other != Agent) Other->GetInteractionComponent()->CancelInteractions();
		const auto Targets = Agent->GetOverlappingAgents();
		for (IInteractionAgentInterface* Target : Targets) OnAgentLeave(Target);
		Agent->SetInteractingAgent(nullptr, true);
	}
	InteractionOwners.Reset();
	SetSelectedTarget(nullptr);
	Super::EndPlay(EndPlayReason);
}

void UInteractionComponent::OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if(!GetInteractionAgent() || GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::None || OtherActor == GetOwner()) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentEnter(OtherInteractionAgent, GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::Passivity);
	}
}

void UInteractionComponent::OnEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if(!GetInteractionAgent() || GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::None || OtherActor == GetOwner()) return;
	if (IsOverlappingActor(OtherActor)) return;

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentLeave(OtherInteractionAgent, GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::Passivity);
	}
}

bool UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(!GetInteractionAgent()->IsInteractable(InInteractionAgent) || GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;
	
	GetInteractionAgent()->GetOverlappingAgents().Add(InInteractionAgent);
	GetInteractionAgent()->OnEnterInteractionRange(InInteractionAgent);
	InInteractionAgent->GetInteractionComponent()->OnAgentEnter(GetInteractionAgent(), !bPassive);
	RefreshTargets();
	return true;
}

bool UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(!GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;

	GetInteractionAgent()->GetOverlappingAgents().Remove(InInteractionAgent);
	GetInteractionAgent()->OnLeaveInteractionRange(InInteractionAgent);
	InInteractionAgent->GetInteractionComponent()->OnAgentLeave(GetInteractionAgent(), !bPassive);
	RefreshTargets();
	return true;
}

TArray<AActor*> UInteractionComponent::GetAvailableTargets() const
{
	TArray<AActor*> Targets;
	IInteractionAgentInterface* Agent = GetInteractionAgent();
	if (!Agent || !IsInteractable() || bEndingPlay) return Targets;
	for (IInteractionAgentInterface* Target : Agent->GetOverlappingAgents())
	{
		AActor* Actor = Cast<AActor>(Target);
		if (!IsValid(Actor) || !Agent->IsInteractable(Target)) continue;
		if (Target->GetInteractingAgent() && Target->GetInteractingAgent() != Agent) continue;
		if (UInteractionComponent* Component = Target->GetInteractionComponent(); Component && !Component->GetOptions(GetOwner()).IsEmpty()) Targets.Add(Actor);
	}
	return Targets;
}

bool UInteractionComponent::NextTarget()
{
	if (GetInteractionAgent()->GetInteractingAgent()) return false;
	const TArray<AActor*> Targets = GetAvailableTargets();
	if (Targets.Num() < 2) return false;
	const int32 Index = Targets.IndexOfByKey(GetSelectedTarget());
	SetSelectedTarget(Targets[(Index + 1) % Targets.Num()]);
	return true;
}

void UInteractionComponent::SetSelectedTarget(AActor* InTarget)
{
	if (SelectedTarget == InTarget) return;
	SelectedTarget = InTarget;
	GetInteractionAgent()->OnInteractionTargetChanged(InTarget);
}

void UInteractionComponent::RefreshTargets()
{
	if (bEndingPlay) return;
	if (bAutoSelectTarget && !GetInteractionAgent()->GetInteractingAgent())
	{
		const auto Targets = GetAvailableTargets();
		if (!Targets.Contains(GetSelectedTarget())) SetSelectedTarget(Targets.IsEmpty() ? nullptr : Targets[0]);
	}
	OnTargetsChanged.Broadcast();
}

void UInteractionComponent::NotifyAvailabilityChanged()
{
	RefreshTargets();
	const auto Agents = GetInteractionAgent()->GetOverlappingAgents();
	for (IInteractionAgentInterface* Agent : Agents) Agent->GetInteractionComponent()->RefreshTargets();
}

bool UInteractionComponent::BeginInteraction(AActor* InInteractor, UObject* InOwner)
{
	IInteractionAgentInterface* Agent = Cast<IInteractionAgentInterface>(InInteractor);
	IInteractionAgentInterface* Target = GetInteractionAgent();
	if (!IsValid(InOwner) || !CanBeginInteraction(InInteractor)) return false;
	InteractionOwners.Add(InOwner);
	if (!Target->GetInteractingAgent()) Target->SetInteractingAgent(Agent);
	NotifyAvailabilityChanged();
	return true;
}

bool UInteractionComponent::CanBeginInteraction(AActor* InInteractor) const
{
	IInteractionAgentInterface* Agent = Cast<IInteractionAgentInterface>(InInteractor);
	IInteractionAgentInterface* Target = GetInteractionAgent();
	return IsValid(InInteractor) && Agent && Target && IsInteractable() && !bEndingPlay && !bCancelling &&
		Agent->GetInteractionComponent()->IsInteractable() && !Agent->GetInteractionComponent()->bEndingPlay && !Agent->GetInteractionComponent()->bCancelling &&
		(!Agent->GetInteractingAgent() || Agent->GetInteractingAgent() == Target) &&
		(!Target->GetInteractingAgent() || Target->GetInteractingAgent() == Agent);
}

void UInteractionComponent::EndInteraction(UObject* InOwner)
{
	if (!InteractionOwners.Remove(InOwner) || !InteractionOwners.IsEmpty()) return;
	IInteractionAgentInterface* Agent = GetInteractionAgent()->GetInteractingAgent();
	GetInteractionAgent()->SetInteractingAgent(nullptr, true);
	if (Agent) Agent->GetInteractionComponent()->RefreshTargets();
	NotifyAvailabilityChanged();
}

bool UInteractionComponent::IsInteractable() const
{
	return GetGenerateOverlapEvents();
}

void UInteractionComponent::SetInteractable(bool bValue)
{
	SetGenerateOverlapEvents(bValue);
	if (!HasBegunPlay()) return;
	if (!bValue)
	{
		IInteractionAgentInterface* Other = GetInteractionAgent()->GetInteractingAgent();
		CancelInteractions();
		if (Other && Other != GetInteractionAgent()) Other->GetInteractionComponent()->CancelInteractions();
	}
	NotifyAvailabilityChanged();
}

IInteractionAgentInterface* UInteractionComponent::GetInteractionAgent() const
{
	return Cast<IInteractionAgentInterface>(GetOwner());
}

FInteractionContext UInteractionComponent::MakeInteractionContext(AActor* InInteractor) const
{
	FInteractionContext Context;
	Context.Interactor = InInteractor;
	Context.Target = GetOwner();
	Context.Player = Cast<APlayerController>(InInteractor);
	if (APawn* Pawn = Cast<APawn>(InInteractor)) Context.Player = Cast<APlayerController>(Pawn->GetController());
	return Context;
}

TArray<FInteractionOptionView> UInteractionComponent::GetOptions(AActor* InInteractor) const
{
	TArray<FInteractionOptionView> Views;
	if (!IsValid(InInteractor) || !IsValid(GetOwner()) || !IsInteractable() || bEndingPlay) return Views;
	const FInteractionContext Context = MakeInteractionContext(InInteractor);
	TSet<FGameplayTag> IDs;
	for (const UInteractionOptionBase* Option : Options)
	{
		if (!Option || !Option->OptionTag.IsValid() || IDs.Contains(Option->OptionTag)) continue;
		IDs.Add(Option->OptionTag);
		if (!Option->IsVisible(Context)) continue;
		FInteractionOptionView View;
		View.OptionTag = Option->OptionTag;
		View.DisplayName = Option->DisplayName;
		View.Priority = Option->Priority;
		View.bEnabled = Option->IsEnabled(Context, View.DisabledReason);
		Views.Add(View);
	}
	Views.StableSort([](const FInteractionOptionView& A, const FInteractionOptionView& B) { return A.Priority > B.Priority; });
	return Views;
}

bool UInteractionComponent::ExecuteOption(AActor* InInteractor, FGameplayTag InOptionTag, FText& OutReason)
{
	if (!InInteractor || ExecutingOptions.Contains(InOptionTag) || GetRunningAction(InOptionTag) || !IsInteractable() || bEndingPlay) return false;
	IInteractionAgentInterface* InteractorAgent = Cast<IInteractionAgentInterface>(InInteractor);
	IInteractionAgentInterface* TargetAgent = GetInteractionAgent();
	if (!InteractorAgent || !TargetAgent || !TargetAgent->IsOverlapping(InteractorAgent))
	{
		OutReason = NSLOCTEXT("Interaction", "OutOfRange", "目标已离开交互范围。");
		return false;
	}
	const auto Views = GetOptions(InInteractor);
	const FInteractionOptionView* View = Views.FindByPredicate([InOptionTag](const FInteractionOptionView& Item) { return Item.OptionTag == InOptionTag; });
	if (!View || !View->bEnabled)
	{
		OutReason = View ? View->DisabledReason : NSLOCTEXT("Interaction", "Unavailable", "选项已不可用。");
		return false;
	}
	bool bSucceeded = false;
	for (const UInteractionOptionBase* Option : Options)
	{
		if (Option && Option->OptionTag == InOptionTag)
		{
			ExecutingOptions.Add(InOptionTag);
			ON_SCOPE_EXIT
			{
				ExecutingOptions.Remove(InOptionTag);
			};
			TArray<UInteractionActionExecution*> Started;
			bSucceeded = true;
			for (UInteractionActionBase* Action : Option->Actions)
			{
				if (!Action) { bSucceeded = false; break; }
				UInteractionActionExecution* Execution = NewObject<UInteractionActionExecution>(this);
				ActiveActions.Add(Execution);
				Started.Add(Execution);
				if (!Execution->Start(Action, MakeInteractionContext(InInteractor), InOptionTag, OutReason)) { bSucceeded = false; break; }
			}
			if (!bSucceeded) for (UInteractionActionExecution* Execution : Started) Execution->Cancel();
			break;
		}
	}
	NotifyOptionsChanged();
	return bSucceeded;
}

UInteractionActionExecution* UInteractionComponent::GetRunningAction(FGameplayTag InOptionTag) const
{
	for (UInteractionActionExecution* Execution : ActiveActions)
	{
		if (Execution->GetOptionTag() == InOptionTag && Execution->GetState() == EInteractionActionState::Running) return Execution;
	}
	return nullptr;
}

void UInteractionComponent::OnActionEnded(UInteractionActionExecution* InExecution)
{
	ActiveActions.Remove(InExecution);
	EndInteraction(InExecution);
	OnOptionsChanged.Broadcast();
}

void UInteractionComponent::CancelInteractions()
{
	if (bCancelling) return;
	TGuardValue<bool> Guard(bCancelling, true);
	const auto Actions = ActiveActions;
	for (UInteractionActionExecution* Execution : Actions) Execution->Cancel();
	OnInteractionCancelled.Broadcast();
}

void UInteractionComponent::FinishActions(FGameplayTag InOptionTag)
{
	const auto Actions = ActiveActions;
	for (UInteractionActionExecution* Execution : Actions) if (Execution->GetOptionTag() == InOptionTag) Execution->Finish();
}

void UInteractionComponent::NotifyOptionsChanged()
{
	OnOptionsChanged.Broadcast();
	NotifyAvailabilityChanged();
}
