// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Audio/AudioModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "AudioModule.generated.h"

class UAudioComponent;
UCLASS()
class WHFRAMEWORK_API AAudioModule : public AModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(AAudioModule)

public:	
	// ParamSets default values for this actor's properties
	AAudioModule();

	~AAudioModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;
	
	virtual void OnTermination_Implementation(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// Sound
public:
	UFUNCTION(BlueprintCallable)
	virtual void PlaySound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlaySound2D(USoundBase* InSound, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable)
	virtual void PlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlaySoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// SingleSound
protected:
	UPROPERTY()
	TMap<FSingleSoundHandle, FSingleSoundInfo> SingleSoundInfos;
	
	UPROPERTY(Replicated)
	int32 SingleSoundHandle;
	
public:
	UFUNCTION(BlueprintPure)
	virtual FSingleSoundInfo GetSingleSoundInfo(const FSingleSoundHandle& InHandle) const;
	
public:
	UFUNCTION(BlueprintCallable)
	virtual FSingleSoundHandle PlaySingleSound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(BlueprintCallable)
	virtual FSingleSoundHandle PlaySingleSound2DWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlaySingleSound2D(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable)
	virtual FSingleSoundHandle PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(BlueprintCallable)
	virtual FSingleSoundHandle PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlaySingleSoundAtLocation(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable)
	virtual void StopSingleSound(const FSingleSoundHandle& InHandle, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopSingleSound(const FSingleSoundHandle& InHandle);

	UFUNCTION(BlueprintCallable)
	virtual void SetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetSingleSoundPaused(const FSingleSoundHandle& InHandle, bool bPaused);

protected:
	virtual UAudioComponent* PlaySingleSound2DImpl(const FSingleSoundHandle& InHandle, USoundBase* InSound, float InVolume = 1.0f);

	virtual UAudioComponent* PlaySingleSoundAtLocationImpl(const FSingleSoundHandle& InHandle, USoundBase* InSound, FVector InLocation, float InVolume = 1.0f);

	virtual void StopSingleSoundImpl(const FSingleSoundHandle& InHandle);

	virtual void SetSingleSoundPausedImpl(const FSingleSoundHandle& InHandle, bool bPaused);

	//////////////////////////////////////////////////////////////////////////
	/// GlobalSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GlobalSound")
	USoundMix* GlobalSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GlobalSound")
	USoundClass* GlobalSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalSoundVolume(float InVolume = 1.f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetGlobalSoundVolume(float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// BackgroundSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "BackgroundSound")
	USoundMix* BackgroundSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "BackgroundSound")
	USoundClass* BackgroundSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetBackgroundSoundVolume(float InVolume = 1.f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetBackgroundSoundVolume(float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "EnvironmentSound")
	USoundMix* EnvironmentSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "EnvironmentSound")
	USoundClass* EnvironmentSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetEnvironmentSoundVolume(float InVolume = 1.f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetEnvironmentSoundVolume(float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// EffectSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "EffectSound")
	USoundMix* EffectSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "EffectSound")
	USoundClass* EffectSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetEffectSoundVolume(float InVolume = 1.f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetEffectSoundVolume(float InVolume);

protected:
	virtual void SetSoundVolumeImpl(USoundMix* InSoundMix, USoundClass* InSoundClass, float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
