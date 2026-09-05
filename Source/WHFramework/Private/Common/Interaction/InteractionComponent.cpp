// Fill out your copyright notice in the Description page of Project Settings.


#include "Common/Interaction/InteractionComponent.h"

#include "Common/Interaction/InteractionAgentInterface.h"
#include "Common/Interaction/InteractionOption.h"
#include "Common/CommonModuleStatics.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

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

	if(IInteractionAgentInterface* OtherInteractionAgent = Cast<IInteractionAgentInterface>(OtherActor))
	{
		OnAgentLeave(OtherInteractionAgent, GetInteractionAgent()->GetInteractAgentType() == EInteractAgentType::Passivity);
	}
}

bool UInteractionComponent::OnAgentEnter(IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(!GetInteractionAgent()->IsInteractable(InInteractionAgent) || GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;
	
	GetInteractionAgent()->GetOverlappingAgents().Add(InInteractionAgent);
	if(!GetInteractionAgent()->GetInteractingAgent())
	{
		GetInteractionAgent()->SetInteractingAgent(InInteractionAgent);
	}
	InInteractionAgent->GetInteractionComponent()->OnAgentEnter(GetInteractionAgent(), !bPassive);
	return true;
}

bool UInteractionComponent::OnAgentLeave(IInteractionAgentInterface* InInteractionAgent, bool bPassive)
{
	if(!GetInteractionAgent()->GetOverlappingAgents().Contains(InInteractionAgent)) return false;

	GetInteractionAgent()->GetOverlappingAgents().Remove(InInteractionAgent);
	if(GetInteractionAgent()->GetInteractingAgent() == InInteractionAgent)
	{
		GetInteractionAgent()->SetInteractingAgent(nullptr);
	}
	InInteractionAgent->GetInteractionComponent()->OnAgentLeave(GetInteractionAgent(), !bPassive);
	if(!GetInteractionAgent()->GetInteractingAgent() && GetInteractionAgent()->GetOverlappingAgents().IsValidIndex(0))
	{
		GetInteractionAgent()->SetInteractingAgent(GetInteractionAgent()->GetOverlappingAgents()[0]);
	}
	return true;
}

bool UInteractionComponent::AddInteractAction(EInteractAction InInteractAction)
{
	if(!InteractActions.Contains(InInteractAction))
	{
		InteractActions.Add(InInteractAction);
		NotifyOptionsChanged();
		return true;
	}
	return false;
}

bool UInteractionComponent::RemoveInteractAction(EInteractAction InInteractAction)
{
	if(InteractActions.Contains(InInteractAction))
	{
		InteractActions.Remove(InInteractAction);
		NotifyOptionsChanged();
		return true;
	}
	return false;
}

void UInteractionComponent::ClearInteractActions()
{
	InteractActions.Empty();
	NotifyOptionsChanged();
}

bool UInteractionComponent::IsInteractable() const
{
	return GetGenerateOverlapEvents();
}

void UInteractionComponent::SetInteractable(bool bValue)
{
	SetGenerateOverlapEvents(bValue);
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
	if (!InInteractor || !IsInteractable()) return Views;
	const FInteractionContext Context = MakeInteractionContext(InInteractor);
	TSet<FName> IDs;
	for (const UInteractionOption* Option : Options)
	{
		if (!Option || Option->OptionID.IsNone() || IDs.Contains(Option->OptionID) || Option->OptionID.ToString().StartsWith(TEXT("Legacy."))) continue;
		IDs.Add(Option->OptionID);
		if (!Option->IsVisible(Context)) continue;
		FInteractionOptionView View;
		View.OptionID = Option->OptionID;
		View.DisplayName = Option->DisplayName;
		View.Priority = Option->Priority;
		View.bEnabled = Option->IsEnabled(Context, View.DisabledReason);
		Views.Add(View);
	}
	IInteractionAgentInterface* TargetAgent = GetInteractionAgent();
	IInteractionAgentInterface* InteractorAgent = Cast<IInteractionAgentInterface>(InInteractor);
	if (TargetAgent && InteractorAgent)
	{
		for (EInteractAction Action : InteractActions)
		{
			if (!TargetAgent->CanInteract(Action, InteractorAgent)) continue;
			FInteractionOptionView View;
			View.OptionID = FName(*FString::Printf(TEXT("Legacy.%d"), (int32)Action));
			View.DisplayName = UCommonModuleStatics::GetEnumDisplayNameByValue(TEXT("/Script/WHFramework.EInteractAction"), (int32)Action);
			View.bEnabled = true;
			View.LegacyAction = Action;
			Views.Add(View);
		}
	}
	Views.StableSort([](const FInteractionOptionView& A, const FInteractionOptionView& B) { return A.Priority > B.Priority; });
	return Views;
}

bool UInteractionComponent::ExecuteOption(AActor* InInteractor, FName InOptionID, FText& OutReason)
{
	if (!InInteractor || bExecutingOption || !IsInteractable()) return false;
	IInteractionAgentInterface* InteractorAgent = Cast<IInteractionAgentInterface>(InInteractor);
	IInteractionAgentInterface* TargetAgent = GetInteractionAgent();
	if (!InteractorAgent || !TargetAgent || !TargetAgent->IsOverlapping(InteractorAgent))
	{
		OutReason = NSLOCTEXT("Interaction", "OutOfRange", "目标已离开交互范围。");
		return false;
	}
	const auto Views = GetOptions(InInteractor);
	const FInteractionOptionView* View = Views.FindByPredicate([InOptionID](const FInteractionOptionView& Item) { return Item.OptionID == InOptionID; });
	if (!View || !View->bEnabled)
	{
		OutReason = View ? View->DisabledReason : NSLOCTEXT("Interaction", "Unavailable", "选项已不可用。");
		return false;
	}
	TGuardValue<bool> Guard(bExecutingOption, true);
	bool bSucceeded = false;
	if (View->LegacyAction != EInteractAction::None)
	{
		bSucceeded = InteractorAgent->DoInteract(View->LegacyAction, TargetAgent);
	}
	else
	{
		for (const UInteractionOption* Option : Options)
		{
			if (Option && Option->OptionID == InOptionID)
			{
				bSucceeded = Option->Execute(MakeInteractionContext(InInteractor), OutReason);
				break;
			}
		}
	}
	NotifyOptionsChanged();
	return bSucceeded;
}

void UInteractionComponent::NotifyOptionsChanged()
{
	OnOptionsChanged.Broadcast();
}
