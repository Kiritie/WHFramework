#include "Audio/AudioModuleNetworkComponent.h"

#include "Audio/AudioModule.h"
#include "GameFramework/PlayerController.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Engine/ChildConnection.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Main/WorldNetworkRoute.h"

namespace
{
	UAudioModule* AuthorizeAudioRequest(UAudioModuleNetworkComponent* Component, const FGuid& Route, USoundBase* Sound)
	{
		if (!::IsValid(Component) || !::IsValid(Sound) || !Component->AcceptRemoteWorldRoute(Route)) return nullptr;
		UWorld* World = Component->GetWorld();
		APlayerController* Controller = Cast<APlayerController>(Component->GetOwner());
		UAudioModule* Audio = UAudioModule::GetPtr(World);
		if (!::IsValid(World) || World->bIsTearingDown || !::IsValid(Controller) || Controller->GetWorld() != World || !::IsValid(Audio)) return nullptr;
		return Audio->CanRequestNetworkSound(Controller, Sound) ? Audio : nullptr;
	}
}

UAudioModuleNetworkComponent::UAudioModuleNetworkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAudioModuleNetworkComponent::OnWorldRouteChanged()
{
	TArray<FSingleSoundHandle> OldSounds;
	if (GetOwner() && GetOwner()->HasAuthority() && AuthoritySoundWorld.IsValid() && AuthoritySoundWorld.Get() == GetWorld())
		for (const auto& Pair : AuthoritySounds) if (Pair.Value.bStarted) OldSounds.Add(Pair.Value.Handle);
	AuthoritySounds.Empty();
	AuthoritySoundWorld.Reset();
	for (auto& Pair : RequestStates)
	{
		if (Pair.Value == EAudioNetworkSoundState::Pending || Pair.Value == EAudioNetworkSoundState::Authorized || Pair.Value == EAudioNetworkSoundState::Started)
		{
			Pair.Value = EAudioNetworkSoundState::Cancelled;
			RequestDispatchResults.FindOrAdd(Pair.Key).Status = ENetworkDispatchStatus::Interrupted;
		}
	}
	ResolvedSounds.Empty();
	PendingDelegates.Empty();
	ForgottenRequests.Empty();
	PendingRequests.Empty();
	RequestStartTimes.Empty();
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld()))
		for (const FSingleSoundHandle& Handle : OldSounds) Audio->StopSingleSound(Handle, true);
}

void UAudioModuleNetworkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MaintainLocalWorldRoute();
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	UAudioModule* Audio = UAudioModule::GetPtr(GetWorld());
	if (Controller && Controller->HasAuthority())
	{
		if (!GetRemoteWorldRoute().IsValid()) return;
		TArray<FGuid> Requests;
		for (const auto& Pair : AuthoritySounds)
			if (Pair.Value.bStartRequested && !Pair.Value.bStarted) Requests.Add(Pair.Key);
		for (const FGuid& Request : Requests) TryStartSingleSound(Request);
		return;
	}
	if (!Controller || !Controller->IsLocalController() || Controller->HasAuthority()) return;
	TArray<FGuid> Expired;
	const double Now = FPlatformTime::Seconds();
	for (const auto& Pair : RequestStartTimes)
	{
		const EAudioNetworkSoundState State = GetRequestState(Pair.Key);
		if (Now - Pair.Value >= 15.0 && (State == EAudioNetworkSoundState::Pending || State == EAudioNetworkSoundState::Authorized)) Expired.Add(Pair.Key);
	}
	for (const FGuid& Request : Expired)
	{
		const FGuid Route = MaintainLocalWorldRoute();
		const EAudioNetworkSoundState State = GetRequestState(Request);
		if (State != EAudioNetworkSoundState::Pending && State != EAudioNetworkSoundState::Authorized) continue;
		if (Route.IsValid()) ServerStopSingleSoundMulticast(Route, Request);
		PendingRequests.Remove(Request);
		PendingDelegates.Remove(Request);
		FSingleSoundHandle Handle;
		if (ResolvedSounds.RemoveAndCopyValue(Request, Handle) && Audio) Audio->CancelSingleSoundCompletion(Handle);
		RequestStates.Add(Request, EAudioNetworkSoundState::TimedOut);
		RequestDispatchResults.FindOrAdd(Request).Status = ENetworkDispatchStatus::TimedOut;
		RequestStartTimes.Remove(Request);
	}
	if (!Audio) return;
	TArray<FGuid> ResolvedRequests;
	ResolvedSounds.GenerateKeyArray(ResolvedRequests);
	for (const FGuid& Request : ResolvedRequests)
	{
		const FSingleSoundHandle* Found = ResolvedSounds.Find(Request);
		if (!Found || !Audio->HasSingleSound(*Found)) continue;
		const FSingleSoundHandle Handle = *Found;
		UAudioComponent* Component = Audio->GetSingleSoundInfo(Handle).Audio;
		if (!IsValid(Component) || Component->GetPlayState() == EAudioComponentPlayState::Stopped)
		{
			ResolvedSounds.Remove(Request);
			RequestStates.Add(Request, EAudioNetworkSoundState::Finished);
			RequestStartTimes.Remove(Request);
			const FGuid Route = MaintainLocalWorldRoute();
			if (Route.IsValid()) ServerReleaseSingleSound(Route, Request);
			Audio->StopSingleSound(Handle);
		}
	}
}

void UAudioModuleNetworkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld()))
			for (const auto& Pair : AuthoritySounds) if (Pair.Value.bStarted) Audio->StopSingleSound(Pair.Value.Handle, true);
	}
	AuthoritySounds.Empty();
	ResolvedSounds.Empty();
	PendingDelegates.Empty();
	ForgottenRequests.Empty();
	PendingRequests.Empty();
	RequestStates.Empty();
	RequestDispatchResults.Empty();
	RequestStartTimes.Empty();
	RequestOrder.Empty();
	Super::EndPlay(EndPlayReason);
}

bool UAudioModuleNetworkComponent::RegisterRequest(const FGuid& InRequestId)
{
	if (!InRequestId.IsValid() || PendingRequests.Num() >= 1024) return false;
	while (RequestOrder.Num() >= 2048)
	{
		const int32 Index = RequestOrder.IndexOfByPredicate([this](const FGuid& Id)
		{
			const EAudioNetworkSoundState State = GetRequestState(Id);
			return State == EAudioNetworkSoundState::Rejected || State == EAudioNetworkSoundState::Finished || State == EAudioNetworkSoundState::Cancelled || State == EAudioNetworkSoundState::TimedOut;
		});
		if (Index == INDEX_NONE) return false;
		RequestStates.Remove(RequestOrder[Index]);
		RequestDispatchResults.Remove(RequestOrder[Index]);
		RequestStartTimes.Remove(RequestOrder[Index]);
		RequestOrder.RemoveAt(Index);
	}
	PendingRequests.Add(InRequestId);
	RequestStates.Add(InRequestId, EAudioNetworkSoundState::Pending);
	FNetworkDispatchResult& Dispatch = RequestDispatchResults.FindOrAdd(InRequestId);
	Dispatch.Status = ENetworkDispatchStatus::RpcDispatched;
	Dispatch.WorldEpoch = WorldNetworkRoute::GetEpoch(GetWorld());
	Dispatch.RemoteDispatched = 1;
	RequestStartTimes.Add(InRequestId, FPlatformTime::Seconds());
	RequestOrder.Add(InRequestId);
	return true;
}

EAudioNetworkSoundState UAudioModuleNetworkComponent::GetRequestState(const FGuid& InRequestId) const
{
	const EAudioNetworkSoundState* State = RequestStates.Find(InRequestId);
	return State ? *State : EAudioNetworkSoundState::Unknown;
}

FNetworkDispatchResult UAudioModuleNetworkComponent::GetRequestDispatchResult(const FGuid& InRequestId) const
{
	const FNetworkDispatchResult* Result = RequestDispatchResults.Find(InRequestId);
	return Result ? *Result : FNetworkDispatchResult::Failure(ENetworkDispatchStatus::InvalidRequest);
}

void UAudioModuleNetworkComponent::TrackCompletion(const FGuid& InRequestId, const FOnSoundPlayFinishDelegate& InDelegate)
{
	if (InRequestId.IsValid() && InDelegate.IsBound()) PendingDelegates.Add(InRequestId, InDelegate);
}

void UAudioModuleNetworkComponent::ForgetRequest(const FGuid& InRequestId)
{
	FSingleSoundHandle Handle;
	if (ResolvedSounds.RemoveAndCopyValue(InRequestId, Handle))
		if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->CancelSingleSoundCompletion(Handle);
	PendingDelegates.Remove(InRequestId);
	if (PendingRequests.Remove(InRequestId)) ForgottenRequests.Add(InRequestId);
	if (RequestStates.Contains(InRequestId)) RequestStates[InRequestId] = EAudioNetworkSoundState::Cancelled;
	if (FNetworkDispatchResult* Result = RequestDispatchResults.Find(InRequestId)) Result->Status = ENetworkDispatchStatus::Interrupted;
	RequestStartTimes.Remove(InRequestId);
}

FSingleSoundHandle UAudioModuleNetworkComponent::ResolveRequest(const FGuid& InRequestId) const
{
	const FSingleSoundHandle* Found = ResolvedSounds.Find(InRequestId);
	return Found ? *Found : FSingleSoundHandle();
}

bool UAudioModuleNetworkComponent::ServerPlaySound2DMulticast_Validate(FGuid WorldRoute, USoundBase* InSound, float InVolume)
{
	return true;
}
void UAudioModuleNetworkComponent::ServerPlaySound2DMulticast_Implementation(FGuid WorldRoute, USoundBase* InSound, float InVolume)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (!IsValid(InSound) || !FMath::IsFinite(InVolume) || InVolume < 0.f) return;
	if (UAudioModule* Audio = AuthorizeAudioRequest(this, WorldRoute, InSound))
		Audio->MultiPlaySound2D(InSound, InVolume);
}

bool UAudioModuleNetworkComponent::ServerPlaySoundAtLocationMulticast_Validate(FGuid WorldRoute, USoundBase* InSound, FVector InLocation, float InVolume)
{
	return true;
}
void UAudioModuleNetworkComponent::ServerPlaySoundAtLocationMulticast_Implementation(FGuid WorldRoute, USoundBase* InSound, FVector InLocation, float InVolume)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (!IsValid(InSound) || InLocation.ContainsNaN() || !FMath::IsFinite(InVolume) || InVolume < 0.f) return;
	if (UAudioModule* Audio = AuthorizeAudioRequest(this, WorldRoute, InSound))
		Audio->MultiPlaySoundAtLocation(InSound, InLocation, InVolume);
}

bool UAudioModuleNetworkComponent::ServerPlaySingleSound2DMulticast_Validate(FGuid WorldRoute, const FGuid& InRequestId, USoundBase* InSound, float InVolume)
{
	return true;
}
void UAudioModuleNetworkComponent::ServerPlaySingleSound2DMulticast_Implementation(FGuid WorldRoute, const FGuid& InRequestId, USoundBase* InSound, float InVolume)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (!InRequestId.IsValid()) return;
	if (!IsValid(InSound) || !FMath::IsFinite(InVolume) || InVolume < 0.f)
	{
		ClientResolveSingleSound(GetRemoteWorldRoute(), InRequestId, FSingleSoundHandle());
		return;
	}
	UAudioModule* Audio = AuthorizeAudioRequest(this, WorldRoute, InSound);
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (!Audio || AuthoritySounds.Contains(InRequestId) || AuthoritySounds.Num() >= 1024)
	{
		ClientResolveSingleSound(GetRemoteWorldRoute(), InRequestId, FSingleSoundHandle());
		return;
	}
	const FSingleSoundHandle Handle = Audio->AllocateNetworkSoundHandle();
	if (Handle.IsValid())
	{
		FAuthoritySound Entry;
		Entry.Handle = Handle;
		Entry.Sound = InSound;
		Entry.Volume = InVolume;
		AuthoritySoundWorld = GetWorld();
		AuthoritySounds.Add(InRequestId, MoveTemp(Entry));
	}
	ClientResolveSingleSound(GetRemoteWorldRoute(), InRequestId, Handle);
}

bool UAudioModuleNetworkComponent::ServerPlaySingleSoundAtLocationMulticast_Validate(FGuid WorldRoute, const FGuid& InRequestId, USoundBase* InSound, FVector InLocation, float InVolume)
{
	return true;
}
void UAudioModuleNetworkComponent::ServerPlaySingleSoundAtLocationMulticast_Implementation(FGuid WorldRoute, const FGuid& InRequestId, USoundBase* InSound, FVector InLocation, float InVolume)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (!InRequestId.IsValid()) return;
	if (!IsValid(InSound) || InLocation.ContainsNaN() || !FMath::IsFinite(InVolume) || InVolume < 0.f)
	{
		ClientResolveSingleSound(GetRemoteWorldRoute(), InRequestId, FSingleSoundHandle());
		return;
	}
	UAudioModule* Audio = AuthorizeAudioRequest(this, WorldRoute, InSound);
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (!Audio || AuthoritySounds.Contains(InRequestId) || AuthoritySounds.Num() >= 1024)
	{
		ClientResolveSingleSound(GetRemoteWorldRoute(), InRequestId, FSingleSoundHandle());
		return;
	}
	const FSingleSoundHandle Handle = Audio->AllocateNetworkSoundHandle();
	if (Handle.IsValid())
	{
		FAuthoritySound Entry;
		Entry.Handle = Handle;
		Entry.Sound = InSound;
		Entry.Location = InLocation;
		Entry.Volume = InVolume;
		Entry.bAtLocation = true;
		AuthoritySoundWorld = GetWorld();
		AuthoritySounds.Add(InRequestId, MoveTemp(Entry));
	}
	ClientResolveSingleSound(GetRemoteWorldRoute(), InRequestId, Handle);
}

bool UAudioModuleNetworkComponent::ServerStopSingleSoundMulticast_Validate(FGuid WorldRoute, const FGuid& InRequestId) { return InRequestId.IsValid(); }
void UAudioModuleNetworkComponent::ServerStopSingleSoundMulticast_Implementation(FGuid WorldRoute, const FGuid& InRequestId)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	FAuthoritySound Sound;
	if (AuthoritySounds.RemoveAndCopyValue(InRequestId, Sound) && Sound.bStarted)
		if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->StopSingleSound(Sound.Handle, true);
}

bool UAudioModuleNetworkComponent::ServerSetSingleSoundPausedMulticast_Validate(FGuid WorldRoute, const FGuid& InRequestId, bool bPaused) { return InRequestId.IsValid(); }
void UAudioModuleNetworkComponent::ServerSetSingleSoundPausedMulticast_Implementation(FGuid WorldRoute, const FGuid& InRequestId, bool bPaused)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	if (FAuthoritySound* Sound = AuthoritySounds.Find(InRequestId))
	{
		Sound->bPaused = bPaused;
		if (Sound->bStarted)
			if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->SetSingleSoundPaused(Sound->Handle, bPaused, true);
	}
}

void UAudioModuleNetworkComponent::ClientResolveSingleSound_Implementation(FGuid WorldRoute, const FGuid& InRequestId, const FSingleSoundHandle& InHandle)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (!InRequestId.IsValid() || ForgottenRequests.Remove(InRequestId)) return;
	if (!PendingRequests.Remove(InRequestId)) return;
	if (InHandle.IsValid() && !InHandle.IsPending())
	{
		UAudioModule* Audio = UAudioModule::GetPtr(GetWorld());
		if (!Audio)
		{
			RequestStates.Add(InRequestId, EAudioNetworkSoundState::Rejected);
			RequestDispatchResults.FindOrAdd(InRequestId).Status = ENetworkDispatchStatus::ModuleUnavailable;
			RequestStartTimes.Remove(InRequestId);
			PendingDelegates.Remove(InRequestId);
			if (AcceptLocalWorldRoute(WorldRoute)) ServerStopSingleSoundMulticast(WorldRoute, InRequestId);
			return;
		}
		ResolvedSounds.Add(InRequestId, InHandle);
		RequestStates.Add(InRequestId, EAudioNetworkSoundState::Authorized);
		if (const FOnSoundPlayFinishDelegate* Delegate = PendingDelegates.Find(InRequestId))
			Audio->BindSingleSoundCompletion(InHandle, *Delegate);
		if (AcceptLocalWorldRoute(WorldRoute)) ServerStartSingleSound(WorldRoute, InRequestId);
	}
	else
	{
		RequestStates.Add(InRequestId, EAudioNetworkSoundState::Rejected);
		RequestDispatchResults.FindOrAdd(InRequestId).Status = ENetworkDispatchStatus::Rejected;
		RequestStartTimes.Remove(InRequestId);
	}
	PendingDelegates.Remove(InRequestId);
}

bool UAudioModuleNetworkComponent::ServerStartSingleSound_Validate(FGuid WorldRoute, const FGuid& InRequestId) { return InRequestId.IsValid(); }
void UAudioModuleNetworkComponent::ServerStartSingleSound_Implementation(FGuid WorldRoute, const FGuid& InRequestId)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	FAuthoritySound* Pending = AuthoritySounds.Find(InRequestId);
	if (!Pending || Pending->bStarted) return;
	Pending->bStartRequested = true;
	TryStartSingleSound(InRequestId);
}

void UAudioModuleNetworkComponent::TryStartSingleSound(const FGuid& InRequestId)
{
	FAuthoritySound* Pending = AuthoritySounds.Find(InRequestId);
	USoundBase* Sound = Pending ? Pending->Sound.Get() : nullptr;
	if (!Pending || !Pending->bStartRequested || Pending->bStarted) return;
	const FGuid OriginalRoute = GetRemoteWorldRoute();
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	UAudioModule* Audio = AuthorizeAudioRequest(this, OriginalRoute, Sound);
	if (!AcceptRemoteWorldRoute(OriginalRoute)) return;
	Pending = AuthoritySounds.Find(InRequestId);
	if (!Pending || !Pending->bStartRequested || Pending->bStarted) return;
	if (!Audio || !Sound)
	{
		AuthoritySounds.Remove(InRequestId);
		ClientRejectSingleSound(GetRemoteWorldRoute(), InRequestId);
		return;
	}
	UNetConnection* Connection = Controller ? Controller->GetNetConnection() : nullptr;
	if (Connection)
	{
		if (UChildConnection* Child = Connection->GetUChildConnection()) Connection = Child->GetParentConnection();
		APlayerController* Parent = Connection ? Connection->PlayerController.Get() : nullptr;
		UAudioModuleNetworkComponent* Receiver = Parent ? Parent->FindComponentByClass<UAudioModuleNetworkComponent>() : nullptr;
		if (!Receiver || Parent->GetWorld() != GetWorld() || !Receiver->GetRemoteWorldRoute().IsValid()) return;
	}
	else return;
	Pending->bStarted = true;
	const FSingleSoundHandle Handle = Pending->Handle;
	const FVector Location = Pending->Location;
	const float Volume = Pending->Volume;
	const bool bAtLocation = Pending->bAtLocation;
	const FNetworkDispatchResult Dispatch = bAtLocation ? Audio->MultiPlaySingleSoundAtLocation(Handle, Sound, Location, Volume) :
		Audio->MultiPlaySingleSound2D(Handle, Sound, Volume);
	Pending = AuthoritySounds.Find(InRequestId);
	if (!Pending || !AcceptRemoteWorldRoute(OriginalRoute)) return;
	if (Dispatch.Status != ENetworkDispatchStatus::LocalDispatched && Dispatch.Status != ENetworkDispatchStatus::RpcDispatched &&
		Dispatch.Status != ENetworkDispatchStatus::PartiallyDispatched)
	{
		AuthoritySounds.Remove(InRequestId);
		ClientRejectSingleSound(OriginalRoute, InRequestId);
		return;
	}
	if (Pending->bPaused) Audio->SetSingleSoundPaused(Handle, true, true);
	if (AcceptRemoteWorldRoute(OriginalRoute)) ClientStartedSingleSound(OriginalRoute, InRequestId, Dispatch);
}

bool UAudioModuleNetworkComponent::ServerReleaseSingleSound_Validate(FGuid WorldRoute, const FGuid& InRequestId) { return InRequestId.IsValid(); }
void UAudioModuleNetworkComponent::ServerReleaseSingleSound_Implementation(FGuid WorldRoute, const FGuid& InRequestId)
{
	if (!AcceptRemoteWorldRoute(WorldRoute)) return;
	AuthoritySounds.Remove(InRequestId);
}

void UAudioModuleNetworkComponent::ClientRejectSingleSound_Implementation(FGuid WorldRoute, const FGuid& InRequestId)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	FSingleSoundHandle Handle;
	if (ResolvedSounds.RemoveAndCopyValue(InRequestId, Handle))
		if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->CancelSingleSoundCompletion(Handle);
	PendingDelegates.Remove(InRequestId);
	PendingRequests.Remove(InRequestId);
	ForgottenRequests.Remove(InRequestId);
	if (RequestStates.Contains(InRequestId)) RequestStates[InRequestId] = EAudioNetworkSoundState::Rejected;
	if (FNetworkDispatchResult* Result = RequestDispatchResults.Find(InRequestId)) Result->Status = ENetworkDispatchStatus::Rejected;
	RequestStartTimes.Remove(InRequestId);
}

void UAudioModuleNetworkComponent::ClientStartedSingleSound_Implementation(FGuid WorldRoute, const FGuid& InRequestId, const FNetworkDispatchResult& InDispatch)
{
	if (!AcceptLocalWorldRoute(WorldRoute) || !InDispatch.WorldEpoch.IsValid() || InDispatch.WorldEpoch != WorldNetworkRoute::GetEpoch(GetWorld()) ||
		(InDispatch.Status != ENetworkDispatchStatus::LocalDispatched && InDispatch.Status != ENetworkDispatchStatus::RpcDispatched &&
			InDispatch.Status != ENetworkDispatchStatus::PartiallyDispatched)) return;
	if (GetRequestState(InRequestId) == EAudioNetworkSoundState::Authorized)
	{
		RequestStates[InRequestId] = EAudioNetworkSoundState::Started;
		RequestDispatchResults.Add(InRequestId, InDispatch);
		RequestStartTimes.Remove(InRequestId);
	}
}

void UAudioModuleNetworkComponent::ClientPlaySound2D_Implementation(FGuid WorldRoute, USoundBase* InSound, float InVolume)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->PlaySound2D(InSound, InVolume);
}

void UAudioModuleNetworkComponent::ClientPlaySoundAtLocation_Implementation(FGuid WorldRoute, USoundBase* InSound, FVector InLocation, float InVolume)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->PlaySoundAtLocation(InSound, InLocation, InVolume);
}

void UAudioModuleNetworkComponent::ClientPlaySingleSound2D_Implementation(FGuid WorldRoute, const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->PlaySingleSound2DImpl(InHandle, InSound, InVolume);
}

void UAudioModuleNetworkComponent::ClientPlaySingleSoundAtLocation_Implementation(FGuid WorldRoute, const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->PlaySingleSoundAtLocationImpl(InHandle, InSound, InLocation, InVolume);
}

void UAudioModuleNetworkComponent::ClientStopSingleSound_Implementation(FGuid WorldRoute, const FSingleSoundHandle& InHandle)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld()))
	{
		Audio->CancelSingleSoundCompletion(InHandle);
		Audio->StopSingleSoundImpl(InHandle);
	}
}

void UAudioModuleNetworkComponent::ClientSetSingleSoundPaused_Implementation(FGuid WorldRoute, const FSingleSoundHandle& InHandle, bool bPaused)
{
	if (!AcceptLocalWorldRoute(WorldRoute)) return;
	if (UAudioModule* Audio = UAudioModule::GetPtr(GetWorld())) Audio->SetSingleSoundPausedImpl(InHandle, bPaused);
}
