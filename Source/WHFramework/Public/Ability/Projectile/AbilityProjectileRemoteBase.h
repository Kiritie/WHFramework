// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Projectile/AbilityProjectileBase.h"
#include "AbilityProjectileRemoteBase.generated.h"

class AAbilityCharacterBase;
class USphereComponent;
class UProjectileMovementComponent;

/**
 * 远程技能基类
 */
UCLASS()
class WHFRAMEWORK_API AAbilityProjectileRemoteBase : public AAbilityProjectileBase
{
	GENERATED_BODY()
	
public:	
	AAbilityProjectileRemoteBase();
	
	//////////////////////////////////////////////////////////////////////////
	/// ObjectPool
public:
	virtual int32 GetLimit_Implementation() const override { return -1; }

	virtual void OnSpawn_Implementation(UObject* InOwner, const TArray<FParameter>& InParams) override;
		
	virtual void OnDespawn_Implementation(bool bRecovery) override;
	
protected:
	UFUNCTION()
	void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	virtual void Launch_Implementation(FVector InDirection) override;
		
public:
	virtual bool CanHitTarget(AActor* InTarget) const override;

	virtual void OnHitTarget(AActor* InTarget, const FHitResult& InHitResult) override;
	
	virtual bool IsHitAble() const override;

	virtual void SetHitAble(bool bValue) override;

protected:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	USphereComponent* SphereComponent;
	
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float InitialVelocity;

public:
	UFUNCTION(BlueprintPure)
	USphereComponent* GetSphereComponent() const { return SphereComponent; }
};
