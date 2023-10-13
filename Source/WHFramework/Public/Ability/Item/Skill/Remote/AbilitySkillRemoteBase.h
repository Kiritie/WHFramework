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

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector InitialVelocity;
	
protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

protected:
	virtual int32 GetLimit_Implementation() const override { return 1000; }

	virtual void OnSpawn_Implementation(const TArray<FParameter>& InParams) override;

	virtual void OnDespawn_Implementation(bool bRecovery) override;

public:
	virtual void Initialize_Implementation(AActor* InOwnerActor, const FAbilityItem& InItem = FAbilityItem::Empty) override;
		
public:
	virtual bool CanHitTarget(AActor* InTarget) const override;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) override;
	
	virtual void ClearHitTargets();

	virtual void SetHitAble(bool bValue);

public:
	UFUNCTION(BlueprintPure)
	USphereComponent* GetSphereComponent() const { return SphereComponent; }

	UFUNCTION(BlueprintPure)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
};
