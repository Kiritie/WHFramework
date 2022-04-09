// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

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

/**
 * ????
 */
UENUM(BlueprintType)
enum class EMouseButton : uint8
{
	Left,
	Middle,
	Right
};

struct WHFRAMEWORK_API FInputModeNone : public FInputModeDataBase
{
	FInputModeNone()
	{}

protected:
	virtual void ApplyInputMode(FReply& SlateOperations, class UGameViewportClient& GameViewportClient) const override;
};

struct WHFRAMEWORK_API FInputModeGameAndUI_NotHideCursor : public FInputModeGameAndUI
{
	FInputModeGameAndUI_NotHideCursor()
	{
		bHideCursorDuringCapture = false;
	}
};
