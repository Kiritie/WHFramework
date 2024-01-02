// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class UWorldWidgetBase;
class UUserWidget;
class UWidgetComponent;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComponentOnTargetLockedOnOff, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FComponentSetRotation, AActor*, TargetActor, FRotator, ControlRotation);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WHFRAMEWORK_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTargetingComponent();

	// The minimum distance to enable target locked on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float MinimumDistanceToEnable = 1200.0f;

	// The AActor Subclass to search for targetable Actors.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TSubclassOf<AActor> TargetableActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TEnumAsByte<ECollisionChannel> TargetableCollisionChannel;

	// Whether or not the character rotation should be controlled when Target is locked on.
	//
	// If true, it'll set the value of bUseControllerRotationYaw and bOrientationToMovement variables on Target locked on / off.
	//
	// Set it to true if you want the character to rotate around the locked on target to enable you to setup strafe animations.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bShouldControlRotation = false;

	// Whether to accept pitch input when bAdjustPitchBasedOnDistanceToTarget is disabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bIgnoreLookInput = true;

	// The amount of time to break line of sight when actor gets behind an Object.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float BreakLineOfSightDelay = 2.0f;

	// Lower this value is, easier it will be to switch new target on right or left. Must be < 1.0f if controlling with gamepad stick
	//
	// When using Sticky Feeling feature, it has no effect (see StickyRotationThreshold)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float StartRotatingThreshold = 0.85f;

	// Whether or not the Target LockOn Widget indicator should be drawn and attached automatically.
	//
	// When set to false, this allow you to manually draw the widget for further control on where you'd like it to appear.
	//
	// OnTargetLockedOn and OnTargetLockedOff events can be used for this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	bool bShouldDrawLockedOnWidget = true;

	// The Widget Class to use when locked on Target. If not defined, will fallback to a Text-rendered
	// widget with a single O character.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	TSubclassOf<UWorldWidgetBase> LockedOnWidgetClass;

	// The Socket name to attach the LockedOn Widget.
	//
	// You should use this to configure the Bone or Socket name the widget should be attached to, and allow
	// the widget to move with target character's animation (Ex: spine_03)
	//
	// Set it to None to attach the Widget Component to the Root Component instead of the Mesh.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	FName LockedOnWidgetParentSocket = FName("spine_03");

	// The Relative Location to apply on Target LockedOn Widget when attached to a target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	FVector LockedOnWidgetRelativeLocation = FVector(0.0f, 0.0f, 0.0f);

	// Setting this to true will tell the Targeting to adjust the Pitch Offset (the Y axis) when locked on,
	// depending on the distance to the target actor.
	//
	// It will ensure that the Camera will be moved up vertically the closer this Actor gets to its target.
	//
	// Formula:
	//
	//   (DistanceToTarget * PitchDistanceCoefficient + PitchDistanceOffset) * -1.0f
	//
	// Then Clamped by PitchMin / PitchMax
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	bool bAdjustPitchBasedOnDistanceToTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchDistanceCoefficient = -0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchDistanceOffset = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchMin = -50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchMax = -20.0f;

	// Set it to true / false whether you want a sticky feeling when switching target
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Sticky Feeling on Target Switch")
	bool bEnableStickyTarget = false;

	// This value gets multiplied to the AxisValue to check against StickyRotationThreshold.
	//
	// Only used when Sticky Target is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Sticky Feeling on Target Switch")
	float AxisMultiplier = 1.0f;

	// Lower this value is, easier it will be to switch new target on right or left.
	//
	// This is similar to StartRotatingThreshold, but you should set this to a much higher value.
	//
	// Only used when Sticky Target is enabled.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Sticky Feeling on Target Switch")
	float StickyRotationThreshold = 30.0f;

	// Function to call to target a new actor.
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void TargetActor();
	
	// Function to call to manually untarget.
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void TargetLockOff();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetShouldControlRotation(bool bValue);

	/**
	* Function to call to switch with X-Axis mouse / controller stick movement.
	*
	* @param AxisValue Pass in the float value of your Input Axis
	*/
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void TargetActorWithAxisInput(float AxisValue);

	// Function to get TargetLocked private variable status
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool GetTargetLockedStatus();

	// Called when a target is locked off, either if it is out of reach (based on MinimumDistanceToEnable) or behind an Object.
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FComponentOnTargetLockedOnOff OnTargetLockedOff;

	// Called when a target is locked on
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FComponentOnTargetLockedOnOff OnTargetLockedOn;

	// Setup the control rotation on Tick when a target is locked on.
	//
	// If not implemented, will fallback to default implementation.
	// If this event is implemented, it lets you control the rotation of the character.
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FComponentSetRotation OnTargetSetRotation;

	// Returns the reference to currently targeted Actor if any
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* GetLockedOnTargetActor() const;

	// Returns true / false whether the system is targeting an actor
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool IsLocked() const;

private:
	UPROPERTY()
	AActor* OwnerActor;

	UPROPERTY()
	APawn* OwnerPawn;

	UPROPERTY()
	APlayerController* OwnerPlayerController;

	UPROPERTY()
	UWorldWidgetBase* TargetLockedOnWidget;

	UPROPERTY()
	AActor* LockedOnTargetActor;

	FTimerHandle LineOfSightBreakTimerHandle;
	FTimerHandle SwitchingTargetTimerHandle;

	bool bIsBreakingLineOfSight = false;
	bool bIsSwitchingTarget = false;
	bool bTargetLocked = false;
	float ClosestTargetDistance = 0.0f;

	bool bDesireToSwitch = false;
	float StartRotatingStack = 0.0f;

	//~ Actors search / trace

	TArray<AActor*> GetAllActorsOfClass(TSubclassOf<AActor> ActorClass) const;
	TArray<AActor*> FindTargetsInRange(TArray<AActor*> ActorsToLook, float RangeMin, float RangeMax) const;

	AActor* FindNearestTarget(TArray<AActor*> Actors) const;

	bool LineTrace(FHitResult& HitResult, const AActor* OtherActor, TArray<AActor*> ActorsToIgnore = TArray<AActor*>()) const;
	bool LineTraceForActor(AActor* OtherActor, TArray<AActor*> ActorsToIgnore = TArray<AActor*>()) const;

	bool ShouldBreakLineOfSight() const;
	void BreakLineOfSight();

	bool IsInViewport(const AActor* TargetActor) const;

	float GetDistanceFromCharacter(const AActor* OtherActor) const;


	//~ Actor rotation

	FRotator GetControlRotationOnTarget(const AActor* OtherActor) const;
	void SetControlRotationOnTarget(AActor* TargetActor) const;
	void ControlRotation(bool ShouldControlRotation) const;

	float GetAngleUsingCameraRotation(const AActor* ActorToLook) const;
	float GetAngleUsingCharacterRotation(const AActor* ActorToLook) const;

	static FRotator FindLookAtRotation(const FVector Start, const FVector Target);

	//~ Widget

	void CreateAndAttachTargetLockedOnWidgetComponent(AActor* TargetActor);

	//~ Targeting

	void TargetLockOn(AActor* TargetToLockOn);
	void ResetIsSwitchingTarget();
	bool ShouldSwitchTargetActor(float AxisValue);

	static bool TargetIsTargetable(const AActor* Actor);

	/**
	 *  Sets up cached Owner PlayerController from Owner Pawn.
	 *
	 *  For local split screen, Pawn's Controller may not have been setup already when this component begins play.
	 */
	 void SetupLocalPlayerController();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
