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
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Components")
	USceneComponent* ScenePoint;

public:
	virtual USceneComponent* GetScenePoint() const override { return ScenePoint; }

	//////////////////////////////////////////////////////////////////////////
	/// Name
protected:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Default")
	FString Name;
public:
	virtual FString GetNameC() const override { return Name; }

	virtual void SetNameC(const FString& InName) override { Name = InName; }

	//////////////////////////////////////////////////////////////////////////
	/// Anim
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Component")
	UCharacterAnim* Anim;
public:
	virtual UCharacterAnim* GetAnim() const override { return Anim; }

	//////////////////////////////////////////////////////////////////////////
	/// Sound
protected:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "Components")
	UAudioComponent* SingleSound;
	
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_SingleSoundParams, Category = "SoundActionInfos")
	FSingleSoundParams SingleSoundParams;

private:
	FTimerHandle SingleSoundStopTimerHandle;

public:
	virtual void PlaySound(class USoundBase* InSound, float InVolume = 1.0f, bool bMulticast = false) override;
	
	virtual void StopSound() override;
	
protected:
	UFUNCTION()
	virtual void OnRep_SingleSoundParams();

	//////////////////////////////////////////////////////////////////////////
	/// Montage
public:
	virtual void PlayMontage(class UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiPlayMontage(class UAnimMontage* InMontage);
	virtual void PlayMontageByName(const FName InMontageName, bool bMulticast = false) override;

	virtual void StopMontage(class UAnimMontage* InMontage, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopMontage(class UAnimMontage* InMontage);
	virtual void StopMontageByName(const FName InMontageName, bool bMulticast = false) override;

	//////////////////////////////////////////////////////////////////////////
	/// Teleport
public:
	virtual void TeleportTo(FTransform InTransform, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiTeleportTo(FTransform InTransform);

	//////////////////////////////////////////////////////////////////////////
	/// AI Move
public:
	virtual void AIMoveTo(FTransform InTransform, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiAIMoveTo(FTransform InTransform);

	virtual void StopAIMove(bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiStopAIMove();

	//////////////////////////////////////////////////////////////////////////
	/// Rotation
public:
	virtual void RotationTowards(FRotator InRotation, float InDuration = 1.f, bool bMulticast = false) override;
	UFUNCTION(NetMulticast, Reliable)
	virtual void MultiRotationTowards(FRotator InRotation, float InDuration = 1.f);

	//////////////////////////////////////////////////////////////////////////
	/// Network
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
