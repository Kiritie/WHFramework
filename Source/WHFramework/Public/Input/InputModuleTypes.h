// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Input/InputTypes.h"
#include "GameFramework/PlayerController.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "UserSettings/EnhancedInputUserSettings.h"

#include "InputModuleTypes.generated.h"

class UInputManagerBase;
class UPlayerMappableKeyProfileBase;

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
struct WHFRAMEWORK_API FInputKeyShortcut
{
	GENERATED_BODY()

public:
	FInputKeyShortcut()
	{
		Keys = TArray<FKey>();
		Auxs = TArray<FKey>();
	}

	FInputKeyShortcut(const TArray<FKey>& InKey)
		: Keys(InKey)
	{
	}

	FInputKeyShortcut(const FText& InDisplayName, const FText& InCategory)
		: DisplayName(InDisplayName),
		  Category(InCategory)
	{
	}

	FInputKeyShortcut(const FText& InDisplayName, const FText& InCategory, const TArray<FKey>& InKey)
		: DisplayName(InDisplayName),
		  Category(InCategory),
		  Keys(InKey),
		  Auxs({})
	{
	}

	FInputKeyShortcut(const FText& InDisplayName, const FText& InCategory, const TArray<FKey>& InKey, const TArray<FKey>& InAuxs)
		: DisplayName(InDisplayName),
		  Category(InCategory),
		  Keys(InKey),
		  Auxs(InAuxs)
	{
	}

public:
	bool IsValid() const
	{
		for(auto& Iter : Keys)
		{
			if(Iter.IsValid())
			{
				return true;
			}
		}
		return false;
	}
		
	bool IsPressed(APlayerController* InPlayerController, bool bAllowInvalid = false) const;
		
	bool IsReleased(APlayerController* InPlayerController, bool bAllowInvalid = false) const;

	bool IsPressing(APlayerController* InPlayerController, bool bAllowInvalid = false) const;

public:
	UPROPERTY(EditAnywhere)
	FText DisplayName;

	UPROPERTY(EditAnywhere)
	FText Category;

	UPROPERTY(EditAnywhere)
	TArray<FKey> Keys;

	UPROPERTY(EditAnywhere)
	TArray<FKey> Auxs;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputContextMapping
{
	GENERATED_BODY()

public:
	FInputContextMapping()
	{
		InputMapping = nullptr;
		Priority = 0;
		bRegisterWithSettings = true;
	}

public:
	UPROPERTY(EditAnywhere, Category="Input")
	UInputMappingContext* InputMapping;

	// Higher priority input mappings will be prioritized over mappings with a lower priority.
	UPROPERTY(EditAnywhere, Category="Input")
	int32 Priority;
	
	/** If true, then this mapping context will be registered with the settings when this game feature action is registered. */
	UPROPERTY(EditAnywhere, Category="Input")
	bool bRegisterWithSettings;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputKeyMapping
{
	GENERATED_BODY()

public:
	FInputKeyMapping()
	{
		Key = FKey();
		Event = EInputEvent::IE_MAX;
	}

	FInputKeyMapping(const FKey& InKey, EInputEvent InEvent, const FInputActionHandlerDynamicSignature& InDelegate)
		: Key(InKey),
		  Event(InEvent),
		  Delegate(InDelegate)
	{
	}
	
public:
	UPROPERTY(BlueprintReadWrite)
	FKey Key;

	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EInputEvent> Event;

	UPROPERTY(BlueprintReadWrite)
	FInputActionHandlerDynamicSignature Delegate;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputTouchMapping
{
	GENERATED_BODY()

public:
	FInputTouchMapping()
	{
		Event = EInputEvent::IE_MAX;
	}

	FInputTouchMapping(EInputEvent InEvent, const FInputTouchHandlerSignature& InDelegate)
		: Event(InEvent),
		  Delegate(InDelegate)
	{
	}

public:
	UPROPERTY(BlueprintReadWrite)
	TEnumAsByte<EInputEvent> Event;

	FInputTouchHandlerSignature Delegate;

	UPROPERTY(BlueprintReadWrite)
	FInputTouchHandlerDynamicSignature DynamicDelegate;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPlayerKeyMappingInfo
{
	GENERATED_BODY()

public:
	FPlayerKeyMappingInfo()
	{
		KeyName = FText::GetEmpty();
		KeyCode = FText::GetEmpty();
		KeyBrushs = TArray<FSlateBrush>();
	}

public:
	UPROPERTY(BlueprintReadOnly)
	FText KeyName;

	UPROPERTY(BlueprintReadOnly)
	FText KeyCode;

	UPROPERTY(BlueprintReadOnly)
	TArray<FSlateBrush> KeyBrushs;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FPlayerInputManagerInfo
{
	GENERATED_BODY()

public:
	FPlayerInputManagerInfo()
	{
	}

public:
	UPROPERTY(Transient)
	TMap<FName, UInputManagerBase*> InputManagerRefs;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputModuleSaveData()
	{
		KeyShortcuts = TMap<FGameplayTag, FInputKeyShortcut>();
		KeyMappings = TMap<FGameplayTag, FKey>();
	}

public:
	UPROPERTY()
	TMap<FGameplayTag, FInputKeyShortcut> KeyShortcuts;

	UPROPERTY()
	TMap<FGameplayTag, FKey> KeyMappings;

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
