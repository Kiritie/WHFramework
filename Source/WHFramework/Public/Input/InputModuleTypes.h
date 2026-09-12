// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Input/InputTypes.h"
#include "GameFramework/PlayerController.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "InputModuleTypes.generated.h"

class UPlayerMappableKeyProfileBase;
class UCommonInputSubsystem;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputUserSettings;
class UInputActionBase;
class UInputComponentBase;
class ULocalPlayer;

UENUM(BlueprintType)
enum class EInputInteractAction : uint8
{
	Primary,
	Secondary,
	Third
};

UENUM(BlueprintType)
enum class EInputInteractEvent : uint8
{
	Started,
	Triggered,
	Completed
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputContextConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input", meta = (Categories = "Input.Context"))
	FGameplayTag ContextTag;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext = nullptr;

	UPROPERTY(EditAnywhere, Category = "Input")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool bAutoActivate = true;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool bRegisterWithSettings = true;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputMappableEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGameplayTag ActionTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName MappingName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	EPlayerMappableKeySlot Slot = EPlayerMappableKeySlot::First;
};

USTRUCT()
struct WHFRAMEWORK_API FInputPlayerRuntime
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputLocalPlayerSubsystem> EnhancedInputSubsystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputUserSettings> UserSettings = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UCommonInputSubsystem> CommonInputSubsystem = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponentBase> InputComponent = nullptr;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputModuleSaveData() = default;


public:
	virtual bool IsValid() const override
	{
		return !Datas.IsEmpty();
	}
	
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
	}
};
