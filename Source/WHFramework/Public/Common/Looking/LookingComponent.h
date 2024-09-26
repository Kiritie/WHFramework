// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "LookingComponent.generated.h"

class UWorldWidgetBase;
class UUserWidget;
class UWidgetComponent;
class APlayerController;

DECLARE_DYNAMIC_DELEGATE_RetVal(bool, FCanLookAtTarget);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTargetLookAtOn, AActor*, TargetActor);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTargetLookAtOff, AActor*, TargetActor);

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
	bool CanLookAtTarget() const;

	UFUNCTION(BlueprintCallable)
	bool DoLookAtTarget(AActor* InTargetActor);

	UFUNCTION(BlueprintPure)
	FRotator GetLookingRotation(AActor* InTargetActor) const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looking")
	float LookingMaxDistance;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Looking")
	float LookingRotationSpeed;

	UPROPERTY(BlueprintReadOnly)
	FCanLookAtTarget OnCanLookAtTarget;

	UPROPERTY(BlueprintReadOnly)
	FTargetLookAtOn OnTargetLookAtOn;

	UPROPERTY(BlueprintReadOnly)
	FTargetLookAtOff OnTargetLookAtOff;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Looking")
	AActor* LookingTarget;
	
	UPROPERTY()
	AActor* OwnerActor;

public:
	UFUNCTION(BlueprintPure)
	AActor* GetLookingTarget() const { return LookingTarget; }
};
