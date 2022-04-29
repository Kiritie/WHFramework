// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CharacterModuleTypes.h"
#include "Base/CharacterInterface.h"
#include "Main/Base/ModuleBase.h"

#include "CharacterModule.generated.h"

UCLASS()
class WHFRAMEWORK_API ACharacterModule : public AModuleBase
{
	GENERATED_BODY()

public:
	// ParamSets default values for this actor's properties
	ACharacterModule();

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
	/// Character
protected:
	UPROPERTY(EditAnywhere, Replicated, Category = "Character")
	TArray<TScriptInterface<ICharacterInterface>> Characters;

public:
	UFUNCTION(BlueprintCallable)
	void AddCharacterToList(TScriptInterface<ICharacterInterface> InCharacter);

	UFUNCTION(BlueprintCallable)
	void RemoveCharacterFromList(TScriptInterface<ICharacterInterface> InCharacter);

	UFUNCTION(BlueprintCallable)
	void RemoveCharacterFromListByName(FName InCharacterName);

	UFUNCTION(BlueprintPure)
	TScriptInterface<ICharacterInterface> GetCharacterByName(FName InCharacterName) const;
	
	//////////////////////////////////////////////////////////////////////////
	/// Sound
public:
	UFUNCTION(BlueprintCallable)
	void PlayCharacterSound(FName InCharacterName, class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable)
	void StopCharacterSound(FName InCharacterName);

	//////////////////////////////////////////////////////////////////////////
	/// Montage
public:
	UFUNCTION(BlueprintCallable)
	void PlayCharacterMontage(FName InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);
	UFUNCTION(BlueprintCallable)
	void PlayCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast = false);
	
	UFUNCTION(BlueprintCallable)
	void StopCharacterMontage(FName InCharacterName, class UAnimMontage* InMontage, bool bMulticast = false);
	UFUNCTION(BlueprintCallable)
	void StopCharacterMontageByName(FName InCharacterName, const FName InMontageName, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// Teleport
public:
	UFUNCTION(BlueprintCallable)
	void TeleportCharacterTo(FName InCharacterName, FTransform InTransform, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// AI Move
public:
	UFUNCTION(BlueprintCallable)
	void AIMoveCharacterTo(FName InCharacterName, FVector InLocation, float InStopDistance = 10.f, bool bMulticast = false);

	UFUNCTION(BlueprintCallable)
	void StopCharacterAIMove(FName InCharacterName, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// Rotation
public:
	UFUNCTION(BlueprintCallable)
	void RotationCharacterTowards(FName InCharacterName, FRotator InRotation, float InDuration = 1.f, bool bMulticast = false);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
