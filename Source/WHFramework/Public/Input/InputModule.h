// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "InputManager.h"
#include "Main/Base/ModuleBase.h"
#include "Input/InputModuleTypes.h"

#include "InputModule.generated.h"

UCLASS()
class WHFRAMEWORK_API AInputModule : public AModuleBase, public IInputManager
{
	GENERATED_BODY()
			
	GENERATED_MODULE(AInputModule)

public:	
	AInputModule();

	~AInputModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnPreparatory_Implementation(EPhase InPhase) override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	virtual void OnTermination_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// Mappings
protected:
	UPROPERTY(EditAnywhere, Category = "Mappings")
	TArray<FInputKeyMapping> KeyMappings;

	UPROPERTY(EditAnywhere, Category = "Mappings")
	TArray<FInputActionMapping> ActionMappings;

	UPROPERTY(EditAnywhere, Category = "Mappings")
	TArray<FInputAxisMapping> AxisMappings;

	UPROPERTY(EditAnywhere, Category = "Mappings")
	TArray<FInputTouchMapping> TouchMappings;

	//////////////////////////////////////////////////////////////////////////
	/// Camera
protected:
	UFUNCTION()
	virtual void TurnCamera(float InRate);

	UFUNCTION()
	virtual void LookUpCamera(float InRate);
	
	UFUNCTION()
	virtual void PanHCamera(float InRate);

	UFUNCTION()
	virtual void PanVCamera(float InRate);

	UFUNCTION()
	virtual void ZoomCamera(float InRate);

	//////////////////////////////////////////////////////////////////////////
	/// Player
protected:
	UFUNCTION()
	virtual void TurnPlayer(float InValue);
	
	UFUNCTION()
	virtual void MoveHPlayer(float InValue);

	UFUNCTION()
	virtual void MoveVPlayer(float InValue);

	UFUNCTION()
	virtual void MoveForwardPlayer(float InValue);

	UFUNCTION()
	virtual void MoveRightPlayer(float InValue);

	UFUNCTION()
	virtual void MoveUpPlayer(float InValue);

	//////////////////////////////////////////////////////////////////////////
	/// Touch
protected:
	UFUNCTION()
	virtual void TouchPressed(ETouchIndex::Type InTouchIndex, FVector InLocation);

	UFUNCTION()
	virtual void TouchPressedImpl();

	UFUNCTION()
	virtual void TouchReleased(ETouchIndex::Type InTouchIndex, FVector InLocation);

	UFUNCTION()
	virtual void TouchReleasedImpl(ETouchIndex::Type InTouchIndex);

	UFUNCTION()
	virtual void TouchMoved(ETouchIndex::Type InTouchIndex, FVector InLocation);

protected:
	UPROPERTY(EditAnywhere, Category = "Touch")
	float TouchInputRate;

	UPROPERTY(VisibleAnywhere, Category = "Touch")
	int32 TouchPressedCount;

	FVector2D TouchLocationPrevious;
	float TouchPinchValuePrevious;
	FTimerHandle TouchReleaseTimerHandle1;
	FTimerHandle TouchReleaseTimerHandle2;
	FTimerHandle TouchReleaseTimerHandle3;

public:
	UFUNCTION(BlueprintPure)
	int32 GetTouchPressedCount() const { return TouchPressedCount; }

	//////////////////////////////////////////////////////////////////////////
	// PlayerController
protected:
	UPROPERTY(VisibleAnywhere)
	AWHPlayerController* PlayerController;
	
public:
	UFUNCTION(BlueprintPure)
	AWHPlayerController* GetPlayerController();

	//////////////////////////////////////////////////////////////////////////
	// InputMode
protected:
	UPROPERTY(EditAnywhere)
	EInputMode NativeInputMode;

	UPROPERTY(VisibleAnywhere)
	EInputMode GlobalInputMode;
	
public:
	UFUNCTION(BlueprintCallable)
	virtual void UpdateInputMode();

	UFUNCTION(BlueprintCallable)
	virtual void SetGlobalInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure)
	EInputMode GetGlobalInputMode() const { return GlobalInputMode; }

	virtual EInputMode GetNativeInputMode() const override { return NativeInputMode; }
};
