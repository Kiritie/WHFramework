// Fill out your copyright notice in the Description page of Project Settings.

#include "Animation/AnimationModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"

#include "Animation/AnimationRequest.h"
#include "Animation/TransitionAction.h"
#include "Animation/Widget/WidgetTransitionMaskBase.h"
#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Main/MainModule.h"
#include "Widget/WidgetModule.h"

IMPLEMENTATION_MODULE(UAnimationModule)

UAnimationModule::UAnimationModule()
{
	ModuleName = FName("AnimationModule");
	ModuleDisplayName = FText::FromString(TEXT("Animation Module"));

	bModuleRequired = true;
}

UAnimationModule::~UAnimationModule()
{
}

#if WITH_EDITOR
void UAnimationModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAnimationModule::OnDestroy()
{
	Super::OnDestroy();

}
#endif

void UAnimationModule::OnInitialize()
{
	Super::OnInitialize();
	bAcceptingRequests = true;
}

void UAnimationModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAnimationModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UAnimationModule::OnPause()
{
	Super::OnPause();
}

void UAnimationModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAnimationModule::OnTermination(EPhase InPhase)
{
	if (PHASEC(InPhase, EPhase::Final))
	{
		bAcceptingRequests = false;
		TArray<FGuid> Ids;
		Requests.GetKeys(Ids);
		TArray<FGuid> PendingIds;
		PendingDispatch.GetKeys(PendingIds);
		Ids.Append(PendingIds);
		for (const FGuid& Id : Ids) CancelRequest(Id);
	}
	Super::OnTermination(InPhase);
}

void UAnimationModule::CancelableDelay(float Duration, ECancelableDelayAction::Type CancelableDelayAction, const FLatentActionInfo& LatentInfo)
{
	UWorld* World = GetWorld();
	if (!bAcceptingRequests || !World || World->bIsTearingDown || !::IsValid(LatentInfo.CallbackTarget)) return;
	if (CancelableDelayAction == ECancelableDelayAction::None && (!FMath::IsFinite(Duration) || Duration < 0.f)) return;
	if (UWorld* OwnerWorld = LatentInfo.CallbackTarget->GetWorld(); OwnerWorld && OwnerWorld != World) return;
	BlueprintDelays.RemoveAll([this](const FBlueprintDelay& Entry)
	{
		return !Entry.Target.IsValid() || (!Requests.Contains(Entry.RequestId) && !PendingDispatch.Contains(Entry.RequestId));
	});
	const FBlueprintDelay* Existing = BlueprintDelays.FindByPredicate([&LatentInfo](const FBlueprintDelay& Entry)
	{
		return Entry.Target.Get() == LatentInfo.CallbackTarget && Entry.UUID == LatentInfo.UUID;
	});
	if (Existing)
	{
		const FGuid ExistingId = Existing->RequestId;
		if (CancelableDelayAction == ECancelableDelayAction::Cancel) CancelRequest(ExistingId);
		return;
	}
	if (CancelableDelayAction != ECancelableDelayAction::None) return;
	const TWeakObjectPtr<UObject> Target = LatentInfo.CallbackTarget;
	const FName FunctionName = LatentInfo.ExecutionFunction;
	const int32 Linkage = LatentInfo.Linkage;
	const FGuid Id = ExecuteWithDelay(LatentInfo.CallbackTarget, Duration, [Target, FunctionName, Linkage]()
	{
		if (UObject* Object = Target.Get())
		{
			if (UFunction* Function = Object->FindFunction(FunctionName))
			{
				int32 ResumeLinkage = Linkage;
				Object->ProcessEvent(Function, &ResumeLinkage);
			}
		}
	});
	if (Id.IsValid()) BlueprintDelays.Add({ Target, LatentInfo.UUID, Id });
}

FGuid UAnimationModule::ExecuteWithDelay(UObject* InOwner, float InDuration, TFunction<void()> InFinish, TFunction<void()> InCancel)
{
	return StartRequest(InOwner, InDuration, nullptr, MoveTemp(InFinish), MoveTemp(InCancel));
}

FGuid UAnimationModule::ExecuteWithTransition(APlayerController* InPlayer, UObject* InOwner, float InDuration, TFunction<void()> InFinish, TFunction<void()> InCancel)
{
	if (!::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld()) return {};
	return StartRequest(InOwner, InDuration, InPlayer, MoveTemp(InFinish), MoveTemp(InCancel));
}

FGuid UAnimationModule::StartRequest(UObject* InOwner, float InDuration, APlayerController* InTransitionPlayer, TFunction<void()> InFinish, TFunction<void()> InCancel)
{
	UWorld* World = GetWorld();
	if (!bAcceptingRequests || !World || World->bIsTearingDown || !::IsValid(InOwner) ||
		!FMath::IsFinite(InDuration) || InDuration < 0.f || NextLatentId == MAX_int32) return {};
	if (UWorld* OwnerWorld = InOwner->GetWorld(); OwnerWorld && OwnerWorld != World) return {};
	const FGuid Id = FGuid::NewGuid();
	UAnimationRequest* Request = NewObject<UAnimationRequest>(this);
	Request->Initialize(*this, *World, *InOwner, Id, MoveTemp(InFinish), MoveTemp(InCancel));
	Requests.Add(Id, Request);
	FLatentActionInfo Info;
	Info.UUID = NextLatentId++;
	Info.Linkage = 0;
	Info.CallbackTarget = Request;
	Info.ExecutionFunction = GET_FUNCTION_NAME_CHECKED(UAnimationRequest, Complete);
	World->GetLatentActionManager().AddNewAction(Request, Info.UUID,
		new FTransitionAction(InTransitionPlayer ? InDuration * .5f : InDuration, Info, InOwner));
	if (InTransitionPlayer)
	{
		if (UWidgetModule* Widgets = UWidgetModule::GetPtr(World))
			Widgets->OpenUserWidget<UWidgetTransitionMaskBase>(InTransitionPlayer, FTransitionMaskWidgetOpenParameter(InDuration));
	}
	return Requests.Contains(Id) ? Id : FGuid();
}

bool UAnimationModule::CancelRequest(const FGuid& InRequestId)
{
	TObjectPtr<UAnimationRequest>* Found = Requests.Find(InRequestId);
	if (!Found) Found = PendingDispatch.Find(InRequestId);
	if (!Found || !::IsValid(*Found)) return false;
	UAnimationRequest* Request = Found->Get();
	if (UWorld* World = GetWorld()) World->GetLatentActionManager().RemoveActionsForObject(Request);
	Request->Cancel();
	return true;
}

void UAnimationModule::FinishRequest(const FGuid& InRequestId, UAnimationRequest* InRequest)
{
	BlueprintDelays.RemoveAll([&InRequestId](const FBlueprintDelay& Entry) { return Entry.RequestId == InRequestId; });
	if (TObjectPtr<UAnimationRequest>* Found = Requests.Find(InRequestId); Found && Found->Get() == InRequest)
		Requests.Remove(InRequestId);
	if (TObjectPtr<UAnimationRequest>* Found = PendingDispatch.Find(InRequestId); Found && Found->Get() == InRequest)
		PendingDispatch.Remove(InRequestId);
}

bool UAnimationModule::QueueRequestCompletion(const FGuid& InRequestId, UAnimationRequest* InRequest)
{
	TObjectPtr<UAnimationRequest>* Found = Requests.Find(InRequestId);
	if (!bAcceptingRequests || !Found || Found->Get() != InRequest || PendingDispatch.Contains(InRequestId)) return false;
	PendingDispatch.Add(InRequestId, InRequest);
	Requests.Remove(InRequestId);
	return true;
}


