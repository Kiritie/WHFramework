// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "GameFramework/PlayerController.h"

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

	bool IsValid() const
	{
		return Key.IsValid();
	}
	
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
		TargetActor = nullptr;
		FuncName = NAME_None;
	}

	FInputKeyMapping(const FKey& InKey, EInputEvent InEvent, AActor* InTargetActor, const FName& InFuncName)
		: Key(InKey),
		  Event(InEvent),
		  TargetActor(InTargetActor),
		  FuncName(InFuncName)
	{
	}
	
public:
	UPROPERTY(EditAnywhere)
	FKey Key;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EInputEvent> Event;

	UPROPERTY(EditAnywhere)
	AActor* TargetActor;

	UPROPERTY(EditAnywhere)
	FName FuncName;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputActionMapping
{
	GENERATED_BODY()

public:
	FInputActionMapping()
	{
		ActionName = NAME_None;
		Event = EInputEvent::IE_MAX;
		TargetActor = nullptr;
		FuncName = NAME_None;
	}

	FInputActionMapping(const FName& InActionName, EInputEvent InEvent, AActor* InTargetActor, const FName& InFuncName)
		: ActionName(InActionName),
		  Event(InEvent),
		  TargetActor(InTargetActor),
		  FuncName(InFuncName)
	{
	}

public:
	UPROPERTY(EditAnywhere)
	FName ActionName;

	UPROPERTY(EditAnywhere)
	TEnumAsByte<EInputEvent> Event;

	UPROPERTY(EditAnywhere)
	AActor* TargetActor;

	UPROPERTY(EditAnywhere)
	FName FuncName;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputAxisMapping
{
	GENERATED_BODY()

public:
	FInputAxisMapping()
	{
		AxisName = NAME_None;
		TargetActor = nullptr;
		FuncName = NAME_None;
	}

	FInputAxisMapping(const FName& InAxisName, AActor* InTargetActor, const FName& InFuncName)
		: AxisName(InAxisName),
		  TargetActor(InTargetActor),
		  FuncName(InFuncName)
	{
	}

public:
	UPROPERTY(EditAnywhere)
	FName AxisName;

	UPROPERTY(EditAnywhere)
	AActor* TargetActor;

	UPROPERTY(EditAnywhere)
	FName FuncName;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FInputTouchMapping
{
	GENERATED_BODY()

public:
	FInputTouchMapping()
	{
		Event = EInputEvent::IE_MAX;
		TargetActor = nullptr;
		FuncName = NAME_None;
	}

	FInputTouchMapping(EInputEvent InEvent, AActor* InTargetActor, const FName& InFuncName)
		: Event(InEvent),
		  TargetActor(InTargetActor),
		  FuncName(InFuncName)
	{
	}

public:
	UPROPERTY(EditAnywhere)
	TEnumAsByte<EInputEvent> Event;

	UPROPERTY(EditAnywhere)
	AActor* TargetActor;

	UPROPERTY(EditAnywhere)
	FName FuncName;
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
