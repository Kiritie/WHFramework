// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AudioModuleTypes.h"

#include "Main/Base/ModuleNetworkComponentBase.h"

#include "AudioModuleNetworkComponent.generated.h"

/**
* 
*/
UCLASS()
class WHFRAMEWORK_API UAudioModuleNetworkComponent : public UModuleNetworkComponentBase
{
	GENERATED_BODY()

public:
	UAudioModuleNetworkComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	FSingleSoundHandle ResolveRequest(const FGuid& InRequestId) const;
	bool RegisterRequest(const FGuid& InRequestId);
	EAudioNetworkSoundState GetRequestState(const FGuid& InRequestId) const;
	FNetworkDispatchResult GetRequestDispatchResult(const FGuid& InRequestId) const;
	void TrackCompletion(const FGuid& InRequestId, const FOnSoundPlayFinishDelegate& InDelegate);
	void ForgetRequest(const FGuid& InRequestId);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySound2DMulticast(FGuid WorldRoute, USoundBase* InSound, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySoundAtLocationMulticast(FGuid WorldRoute, USoundBase* InSound, FVector InLocation, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySingleSound2DMulticast(FGuid WorldRoute, const FGuid& InRequestId, USoundBase* InSound, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerPlaySingleSoundAtLocationMulticast(FGuid WorldRoute, const FGuid& InRequestId, USoundBase* InSound, FVector InLocation, float InVolume);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerStopSingleSoundMulticast(FGuid WorldRoute, const FGuid& InRequestId);

	UFUNCTION(BlueprintCallable, Server, Reliable, WithValidation)
	void ServerSetSingleSoundPausedMulticast(FGuid WorldRoute, const FGuid& InRequestId, bool bPaused);

	UFUNCTION(Client, Reliable)
	void ClientResolveSingleSound(FGuid WorldRoute, const FGuid& InRequestId, const FSingleSoundHandle& InHandle);
	UFUNCTION(Client, Reliable)
	void ClientRejectSingleSound(FGuid WorldRoute, const FGuid& InRequestId);
	UFUNCTION(Client, Reliable)
	void ClientStartedSingleSound(FGuid WorldRoute, const FGuid& InRequestId, const FNetworkDispatchResult& InDispatch);

	UFUNCTION(Client, Reliable)
	void ClientPlaySound2D(FGuid WorldRoute, USoundBase* InSound, float InVolume);
	UFUNCTION(Client, Reliable)
	void ClientPlaySoundAtLocation(FGuid WorldRoute, USoundBase* InSound, FVector InLocation, float InVolume);
	UFUNCTION(Client, Reliable)
	void ClientPlaySingleSound2D(FGuid WorldRoute, const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume);
	UFUNCTION(Client, Reliable)
	void ClientPlaySingleSoundAtLocation(FGuid WorldRoute, const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume);
	UFUNCTION(Client, Reliable)
	void ClientStopSingleSound(FGuid WorldRoute, const FSingleSoundHandle& InHandle);
	UFUNCTION(Client, Reliable)
	void ClientSetSingleSoundPaused(FGuid WorldRoute, const FSingleSoundHandle& InHandle, bool bPaused);

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerStartSingleSound(FGuid WorldRoute, const FGuid& InRequestId);
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerReleaseSingleSound(FGuid WorldRoute, const FGuid& InRequestId);

protected:
	virtual void OnWorldRouteChanged() override;

private:
	struct FAuthoritySound
	{
		FSingleSoundHandle Handle;
		TWeakObjectPtr<USoundBase> Sound;
		FVector Location = FVector::ZeroVector;
		float Volume = 1.f;
		bool bAtLocation = false;
		bool bStarted = false;
		bool bPaused = false;
		bool bStartRequested = false;
	};
	void TryStartSingleSound(const FGuid& InRequestId);
	TMap<FGuid, FAuthoritySound> AuthoritySounds;
	TWeakObjectPtr<UWorld> AuthoritySoundWorld;
	TMap<FGuid, FSingleSoundHandle> ResolvedSounds;
	TMap<FGuid, FOnSoundPlayFinishDelegate> PendingDelegates;
	TSet<FGuid> ForgottenRequests;
	TSet<FGuid> PendingRequests;
	TMap<FGuid, EAudioNetworkSoundState> RequestStates;
	TMap<FGuid, FNetworkDispatchResult> RequestDispatchResults;
	TMap<FGuid, double> RequestStartTimes;
	TArray<FGuid> RequestOrder;
};
