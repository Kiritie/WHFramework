// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "InputManager.h"
#include "Base/ModuleBase.h"

#include "InputModule.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnChangeInputMode, EInputMode, InInputMode);

UCLASS()
class WHFRAMEWORK_API AInputModule : public AModuleBase, public IInputManager
{
	GENERATED_BODY()
	
public:	
	AInputModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
#if WITH_EDITOR
	virtual void OnGenerate_Implementation() override;

	virtual void OnDestroy_Implementation() override;
#endif
	
	virtual void OnInitialize_Implementation() override;

	virtual void OnRefresh_Implementation(float DeltaSeconds) override;

	virtual void OnPause_Implementation() override;

	virtual void OnUnPause_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// InputMode
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EInputMode InputMode;

public:
	UPROPERTY(BlueprintCallable)
	FOnChangeInputMode OnChangeInputMode;

public:
	UFUNCTION(BlueprintCallable)
	void UpdateInputMode();

	UFUNCTION(BlueprintCallable)
	void SetInputMode(EInputMode InInputMode);

	UFUNCTION(BlueprintPure)
	EInputMode GetInputMode() const { return InputMode; }

	virtual EInputMode GetNativeInputMode() const override;
};
