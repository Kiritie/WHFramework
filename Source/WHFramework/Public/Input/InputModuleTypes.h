// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "GameFramework/PlayerController.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "EnhancedActionKeyMapping.h"

#include "InputModuleTypes.generated.h"

/**
* 输入模式
*/
UENUM(BlueprintType)
enum class EInputMode : uint8
{
	/// 无
	None,
	/// 游戏模式
	GameOnly,
	/// UI模式
	UIOnly,
	/// 游戏和UI模式
	GameAndUI,
	/// 游戏和UI模式_不隐藏光标
	GameAndUI_NotHideCursor
};

UENUM(BlueprintType)
enum class EInputInteractAction : uint8
{
	Action1,
	Action2,
	Action3
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputKeyShortcut
{
	GENERATED_BODY()

public:
	FInputKeyShortcut()
	{
		Key = FKey();
		Auxs = TArray<FKey>();
	}

	FInputKeyShortcut(const FKey& InKey)
		: Key(InKey),
		  Auxs({})
	{
	}

	FInputKeyShortcut(const FKey& InKey, const TArray<FKey>& InAuxs)
		: Key(InKey),
		  Auxs(InAuxs)
	{
	}

public:
	bool IsValid() const
	{
		return Key.IsValid();
	}
	
	bool IsPressing(APlayerController* InPlayerController, bool bAllowInvalid = false) const;

public:
	UPROPERTY(EditAnywhere)
	FKey Key;

	UPROPERTY(EditAnywhere)
	TArray<FKey> Auxs;
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

	FInputKeyMapping(const FKey& InKey, EInputEvent InEvent, const FInputActionHandlerWithKeySignature& InDelegate)
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

	FInputActionHandlerWithKeySignature Delegate;

	UPROPERTY(BlueprintReadWrite)
	FInputActionHandlerDynamicSignature DynamicDelegate;
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

struct WHFRAMEWORK_API FInputModeNone : public FInputModeDataBase
{
	FInputModeNone()
	{}

protected:
	virtual void ApplyInputMode(class FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

struct WHFRAMEWORK_API FInputModeGameAndUI_NotHideCursor : public FInputModeGameAndUI
{
	FInputModeGameAndUI_NotHideCursor()
	{
		bHideCursorDuringCapture = false;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputActionMappings : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputActionMappings()
	{
		Mappings = TArray<FEnhancedActionKeyMapping>();
	}

public:
	UPROPERTY()
	TArray<FEnhancedActionKeyMapping> Mappings;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FInputModuleSaveData()
	{
		ActionMappings = TArray<FInputActionMappings>();
		KeyShortcuts = TMap<FName, FKey>();
		KeyMappings = TMap<FName, FKey>();
	}

public:
	UPROPERTY()
	TArray<FInputActionMappings> ActionMappings;

	UPROPERTY()
	TMap<FName, FKey> KeyShortcuts;

	UPROPERTY()
	TMap<FName, FKey> KeyMappings;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		for(auto& Iter : ActionMappings)
		{
			Iter.MakeSaved();
		}
	}

	virtual TArray<FEnhancedActionKeyMapping*> GetActionMappingsByName(const FName InActionName);
};
