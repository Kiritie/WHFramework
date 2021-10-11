// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "CharacterInterface.h"
#include "GameFramework/Character.h"
#include "Audio/AudioModuleTypes.h"
#include "Scene/Object/SceneObject.h"

#include "CharacterBase.generated.h"

/**
 * 
 */
UCLASS(Blueprintable)
class WHFRAMEWORK_API ACharacterBase : public ACharacter, public ICharacterInterface, public ISceneObject
{
	GENERATED_BODY()
	
public:
	ACharacterBase();

protected:
	virtual void BeginPlay() override;
	
public:
	virtual void Tick(float DeltaSeconds) override;

	//////////////////////////////////////////////////////////////////////////
	/// Scene
protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Components")
	USceneComponent* ScenePoint;

public:
	virtual USceneComponent* GetScenePoint_Implementation() const override;

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	FName CharacterName;
public:
	virtual FName GetCharacterName_Implementation() const override;

	//////////////////////////////////////////////////////////////////////////
	/// Anim
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Component")
	UCharacterAnim* CharacterAnim;
public:
	virtual UCharacterAnim* GetCharacterAnim_Implementation() const override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Speak
public:
	virtual void StartSpeak_Implementation() override;

	virtual void StopSpeak_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	/// Sound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleDefaultsOnly, Category = "Components")
	UAudioComponent* SingleSound;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, ReplicatedUsing = OnRep_SingleSoundParams, Category = "SoundActionInfos")
	FSingleSoundParams SingleSoundParams;

private:
	FTimerHandle SingleSoundStopTimerHandle;

public:
	virtual void PlaySound_Implementation(class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) override;
	
	virtual void StopSound_Implementation() override;
	
protected:
	UFUNCTION()
	virtual void OnRep_SingleSoundParams();

	//////////////////////////////////////////////////////////////////////////
	/// Montage
public:
	virtual void PlayMontage_Implementation(class UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlayMontage(class UAnimMontage* InMontage);
	virtual void PlayMontageByName_Implementation(const FName InMontageName, bool bMulticast = false) override;

	virtual void StopMontage_Implementation(class UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopMontage(class UAnimMontage* InMontage);
	virtual void StopMontageByName_Implementation(const FName InMontageName, bool bMulticast = false) override;

	//////////////////////////////////////////////////////////////////////////
	/// Teleport
public:
	virtual void TeleportTo_Implementation(FTransform InTransform, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiTeleportTo(FTransform InTransform);

	//////////////////////////////////////////////////////////////////////////
	/// AI Move
public:
	virtual void AIMoveTo_Implementation(FTransform InTransform, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiAIMoveTo(FTransform InTransform);

	virtual void StopAIMove_Implementation(bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopAIMove();

	//////////////////////////////////////////////////////////////////////////
	/// Rotation
public:
	virtual void RotationTowards_Implementation(FRotator InRotation, float InDuration = 1.f, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiRotationTowards(FRotator InRotation, float InDuration = 1.f);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
