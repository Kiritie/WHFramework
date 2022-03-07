// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "Audio/AudioModuleTypes.h"
#include "Main/Base/ModuleBase.h"

#include "AudioModule.generated.h"

class UAudioComponent;
UCLASS()
class WHFRAMEWORK_API AAudioModule : public AModuleBase
{
	GENERATED_BODY()
	
public:	
	// ParamSets default values for this actor's properties
	AAudioModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif

	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;
	
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
	/// Single Sound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Components")
	UAudioComponent* SingleSoundComponent;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing = OnRep_SingleSoundParams, Category = "BGSound")
	FSingleSoundParams SingleSoundParams;

private:
	FTimerHandle SingleSoundStopTimerHandle;

public:
	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleSound2D(USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleSoundAtLocation(USoundBase* InSound, FVector InLocation, float InVolume = 1.0f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	virtual void StopSingleSound();

protected:
	UFUNCTION()
	virtual void OnRep_SingleSoundParams();

	//////////////////////////////////////////////////////////////////////////
	/// Global BGSound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Components")
	UAudioComponent* GlobalBGSoundComponent;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "BGSound")
	bool bAutoPlayGlobalBGSound;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, ReplicatedUsing = OnRep_GlobalBGSoundParams, Category = "BGSound")
	FBGSoundParams GlobalBGSoundParams;

private:
	FTimerHandle GlobalBGSoundLoopTimerHandle;

public:
	UFUNCTION(BlueprintCallable)
	virtual void PlayGlobalBGSound();

	UFUNCTION(BlueprintCallable)
	virtual void PauseGlobalBGSound();

	UFUNCTION(BlueprintCallable)
	virtual void StopGlobalBGSound();

protected:
	UFUNCTION()
	virtual void OnLoopGlobalBGSound();

	UFUNCTION()
	virtual void OnRep_GlobalBGSoundParams();

	//////////////////////////////////////////////////////////////////////////
	/// Single BGSound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Components")
	UAudioComponent* SingleBGSoundComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing = OnRep_SingleBGSoundParams, Category = "BGSound")
	FBGSoundParams SingleBGSoundParams;

private:
	FTimerHandle SingleBGSoundLoopTimerHandle;

public:
	UFUNCTION(BlueprintCallable)
	virtual void InitSingleBGSound(USoundBase* InBGSound, float InBGVolume = 1.0f, bool bIsLoopSound = true, bool bIsUISound = false, bool bIsAutoPlay = true);

	UFUNCTION(BlueprintCallable)
	virtual void PlaySingleBGSound();

	UFUNCTION(BlueprintCallable)
	virtual void PauseSingleBGSound();

	UFUNCTION(BlueprintCallable)
	virtual void StopSingleBGSound();

protected:
	UFUNCTION()
	virtual void OnLoopSingleBGSound();

	UFUNCTION()
	virtual void OnRep_SingleBGSoundParams();

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
