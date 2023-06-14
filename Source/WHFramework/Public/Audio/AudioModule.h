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
	
	virtual void OnTermination_Implementation() override;

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
	TMap<FName, UAudioComponent*> AudioComponents;
	
public:
	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleSound2D(USoundBase* InSound, FName InFlag, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlaySingleSound2D(USoundBase* InSound, FName InFlag, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleSoundAtLocation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlaySingleSoundAtLocation(USoundBase* InSound, FName InFlag, FVector InLocation, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleSound2DWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, float InVolume = 1.0f);
	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleSoundAtLocationWithDelegate(USoundBase* InSound, FName InFlag, const FOnSoundPlayFinishDelegate& InOnSoundPlayFinish, FVector InLocation, float InVolume = 1.0f);

	UFUNCTION(BlueprintCallable)
	virtual void StopSingleSound(FName InFlag, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopSingleSound(FName InFlag);

	UFUNCTION(BlueprintCallable)
	virtual void SetSingleSoundPaused(FName InFlag, bool bPaused, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetSingleSoundPaused(FName InFlag, bool bPaused);

protected:
	virtual void NativePlaySingleSound(FName InFlag, UAudioComponent* InAudioComponent);

	virtual void NativeStopSingleSound(FName InFlag);

	virtual void NativeSetSingleSoundPaused(FName InFlag, bool bPaused);

	//////////////////////////////////////////////////////////////////////////
	/// GlobalSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GlobalSound")
	USoundMix* GlobalSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GlobalSound")
	USoundClass* GlobalSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalSoundVolume(float InVolume, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetGlobalSoundVolume(float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// BGMSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "BGMSound")
	USoundMix* BGMSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "BGMSound")
	USoundClass* BGMSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetBGMSoundVolume(float InVolume, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetBGMSoundVolume(float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// EnvironmentSound
protected:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "EnvironmentSound")
	USoundMix* EnvironmentSoundMix;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "EnvironmentSound")
	USoundClass* EnvironmentSoundClass;

public:
	UFUNCTION(BlueprintCallable)
	virtual void SetEnvironmentSoundVolume(float InVolume, bool bMulticast = false);
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
	virtual void SetEffectSoundVolume(float InVolume, bool bMulticast = false);
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiSetEffectSoundVolume(float InVolume);

protected:
	virtual void NativeSetSoundVolume(USoundMix* InSoundMix, USoundClass* InSoundClass, float InVolume);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
