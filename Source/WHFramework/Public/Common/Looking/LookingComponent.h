// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "LookingComponent.generated.h"

class UWorldWidgetBase;
class UUserWidget;
class UWidgetComponent;
class APlayerController;

DECLARE_DYNAMIC_DELEGATE_RetVal(bool, FComponentCanLockAtTarget);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WHFRAMEWORK_API ULookingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULookingComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	UFUNCTION(BlueprintCallable)
	void TargetLookingOn(AActor* InTargetActor);

	UFUNCTION(BlueprintCallable)
	void TargetLookingOff();

	UFUNCTION(BlueprintPure)
	bool TargetIsLookAtAble(AActor* InTargetActor) const;

	UFUNCTION(BlueprintPure)
	bool CanLookAtTarget();

	UFUNCTION(BlueprintCallable)
	bool DoLookAtTarget(AActor* InTargetActor);

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looking")
	float LookingMaxDistance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looking")
	float LookingRotationSpeed;

	UPROPERTY(BlueprintReadOnly)
	FComponentCanLockAtTarget OnCanLockAtTarget;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Looking")
	AActor* LookingTarget;
	
	UPROPERTY()
	AActor* OwnerActor;

public:
	UFUNCTION(BlueprintPure)
	AActor* GetLookingTarget() const { return LookingTarget; }
};
