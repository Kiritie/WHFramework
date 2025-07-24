// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "InputTypes.generated.h"

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
	/// 游戏模式_不隐藏光标
	GameOnly_NotHideCursor,
	/// 游戏和UI模式
	GameAndUI,
	/// 游戏和UI模式_不隐藏光标
	GameAndUI_NotHideCursor,
	/// UI模式
	UIOnly
};

struct WHFRAMEWORKCORE_API FInputModeNone : public FInputModeDataBase
{
	FInputModeNone()
	{}

protected:
	virtual void ApplyInputMode(class FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

struct WHFRAMEWORKCORE_API FInputModeGameOnly_NotHideCursor : public FInputModeGameOnly
{
	FInputModeGameOnly_NotHideCursor()
	{
		bConsumeCaptureMouseDown = false;
	}
};

struct WHFRAMEWORKCORE_API FInputModeGameAndUI_NotHideCursor : public FInputModeGameAndUI
{
	FInputModeGameAndUI_NotHideCursor()
	{
		bHideCursorDuringCapture = false;
	}
};
