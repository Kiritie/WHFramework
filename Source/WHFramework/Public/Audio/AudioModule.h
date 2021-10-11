// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "AudioModuleTypes.h"
#include "Base/ModuleBase.h"

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

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

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
	/// Game BGSound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Components")
	UAudioComponent* GameBGSoundComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing = OnRep_GameBGSoundParams, Category = "BGSound")
	FBGSoundParams GameBGSoundParams;

private:
	FTimerHandle GameBGSoundLoopTimerHandle;

public:
	UFUNCTION(BlueprintCallable)
	virtual void InitGameBGSound(USoundBase* InBGSound, float InBGVolume = 1.0f, bool bIsLoopSound = true, bool bIsUISound = false, bool bIsAutoPlay = true);

	UFUNCTION(BlueprintCallable)
	virtual void PlayGameBGSound();

	UFUNCTION(BlueprintCallable)
	virtual void PauseGameBGSound();

	UFUNCTION(BlueprintCallable)
	virtual void StopGameBGSound();

protected:
	UFUNCTION()
	virtual void OnLoopGameBGSound();

	UFUNCTION()
	virtual void OnRep_GameBGSoundParams();

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
