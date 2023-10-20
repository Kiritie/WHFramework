// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Audio/AudioModuleTypes.h"
#include "Main/Base/ModuleBase.h"
#include "SaveGame/Base/SaveDataInterface.h"

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

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) override;

	virtual void UnloadData(EPhase InPhase) override;

	virtual FSaveData* ToData() override;

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GlobalSound")
	USoundMix* GlobalSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GlobalSound")
	USoundClass* GlobalSoundClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GlobalSound")
	FSoundParams GlobalSoundParams;

public:
	UFUNCTION(BlueprintPure)
	virtual float GetGlobalSoundVolume() const { return GlobalSoundParams.Volume; }

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f)
	{
		GlobalSoundParams.Volume = InVolume;
		SetSoundParams(GlobalSoundMix, GlobalSoundClass, GlobalSoundParams, InFadeInTime);
	}

	UFUNCTION(BlueprintPure)
	virtual float GetGlobalSoundPitch() const { return GlobalSoundParams.Pitch; }

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f)
	{
		GlobalSoundParams.Pitch = InPitch;
		SetSoundParams(GlobalSoundMix, GlobalSoundClass, GlobalSoundParams, InFadeInTime);
	}

	//////////////////////////////////////////////////////////////////////////
	/// BackgroundSound
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackgroundSound")
	USoundMix* BackgroundSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackgroundSound")
	USoundClass* BackgroundSoundClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "BackgroundSound")
	FSoundParams BackgroundSoundParams;

public:
	UFUNCTION(BlueprintPure)
	virtual float GetBackgroundSoundVolume() const { return BackgroundSoundParams.Volume; }

	UFUNCTION(BlueprintCallable)
	virtual void SetBackgroundSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f)
	{
		BackgroundSoundParams.Volume = InVolume;
		SetSoundParams(BackgroundSoundMix, BackgroundSoundClass, BackgroundSoundParams, InFadeInTime);
	}

	UFUNCTION(BlueprintPure)
	virtual float GetBackgroundSoundPitch() const { return BackgroundSoundParams.Pitch; }

	UFUNCTION(BlueprintCallable)
	virtual void SetBackgroundSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f)
	{
		BackgroundSoundParams.Pitch = InPitch;
		SetSoundParams(BackgroundSoundMix, BackgroundSoundClass, BackgroundSoundParams, InFadeInTime);
	}

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnvironmentSound")
	USoundMix* EnvironmentSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnvironmentSound")
	USoundClass* EnvironmentSoundClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EnvironmentSound")
	FSoundParams EnvironmentSoundParams;

public:
	UFUNCTION(BlueprintPure)
	virtual float GetEnvironmentSoundVolume() const { return EnvironmentSoundParams.Volume; }

	UFUNCTION(BlueprintCallable)
	virtual void SetEnvironmentSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f)
	{
		EnvironmentSoundParams.Volume = InVolume;
		SetSoundParams(EnvironmentSoundMix, EnvironmentSoundClass, EnvironmentSoundParams, InFadeInTime);
	}

	UFUNCTION(BlueprintPure)
	virtual float GetEnvironmentSoundPitch() const { return EnvironmentSoundParams.Pitch; }

	UFUNCTION(BlueprintCallable)
	virtual void SetEnvironmentSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f)
	{
		EnvironmentSoundParams.Pitch = InPitch;
		SetSoundParams(EnvironmentSoundMix, EnvironmentSoundClass, EnvironmentSoundParams, InFadeInTime);
	}

	//////////////////////////////////////////////////////////////////////////
	/// EffectSound
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EffectSound")
	USoundMix* EffectSoundMix;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EffectSound")
	USoundClass* EffectSoundClass;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "EffectSound")
	FSoundParams EffectSoundParams;

public:
	UFUNCTION(BlueprintPure)
	virtual float GetEffectSoundVolume() const { return EffectSoundParams.Volume; }

	UFUNCTION(BlueprintCallable)
	virtual void SetEffectSoundVolume(float InVolume = 1.f, float InFadeInTime = 1.0f)
	{
		EffectSoundParams.Volume = InVolume;
		SetSoundParams(EffectSoundMix, EffectSoundClass, EffectSoundParams, InFadeInTime);
	}

	UFUNCTION(BlueprintPure)
	virtual float GetEffectSoundPitch() const { return EffectSoundParams.Pitch; }

	UFUNCTION(BlueprintCallable)
	virtual void SetEffectSoundPitch(float InPitch = 1.f, float InFadeInTime = 1.0f)
	{
		EffectSoundParams.Pitch = InPitch;
		SetSoundParams(EffectSoundMix, EffectSoundClass, EffectSoundParams, InFadeInTime);
	}

protected:
	virtual void SetSoundParams(USoundMix* InSoundMix, USoundClass* InSoundClass, const FSoundParams& InParams, float InFadeInTime = 1.0f);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
