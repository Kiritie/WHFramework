// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Ability/Item/Skill/AbilitySkillBase.h"
#include "AbilitySkillRemoteBase.generated.h"

class AAbilityCharacterBase;
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

/**
 * Զ�̼���
 */
UCLASS()
class WHFRAMEWORK_API AAbilitySkillRemoteBase : public AAbilitySkillBase
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
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

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	virtual void Initialize(AAbilityCharacterBase* InOwnerCharacter) override;

	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION(BlueprintImplementableEvent)
	void OnHitTarget(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
		
	UFUNCTION(BlueprintPure)
	USphereComponent* GetSphereComponent() const { return SphereComponent; }

	UFUNCTION(BlueprintPure)
	UStaticMeshComponent* GetMeshComponent() const { return MeshComponent; }
};
