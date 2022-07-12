// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/Skill/AbilitySkillBase.h"
#include "AbilitySkillRemoteBase.generated.h"

class AAbilityCharacterBase;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * 远程技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilitySkillRemoteBase : public AAbilitySkillBase
{
	GENERATED_BODY()
	
public:	
	AAbilitySkillRemoteBase();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* MeshComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* MovementComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	FVector InitialVelocity;

public:
	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter) override;

protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintNativeEvent)
	void OnHitTarget(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	UFUNCTION(BlueprintPure)
	USphereComponent* GetSphereComponent() const { return SphereComponent; }

	UFUNCTION(BlueprintPure)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
};
