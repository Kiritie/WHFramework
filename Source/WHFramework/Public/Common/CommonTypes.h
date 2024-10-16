// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "WHFrameworkCoreTypes.h"

#include "CommonTypes.generated.h"

//////////////////////////////////////////////////////////////////////////
// Variables
extern WHFRAMEWORK_API bool GIsPlaying;
extern WHFRAMEWORK_API bool GIsSimulating;

//////////////////////////////////////////////////////////////////////////
// Types
UENUM(BlueprintType)
enum class EPauseMode : uint8
{
	Default,
	TimeOnly,
	ModuleOnly,
	TimeAndModule
};

UENUM(BlueprintType)
enum class EPhase : uint8
{
	None = 0,
	Primary = 1 << 0,
	Lesser = 1 << 1,
	Final = 1 << 2,
	PrimaryAndLesser = Primary | Lesser,
	LesserAndFinal = Lesser | Final,
	PrimaryAndFinal = Primary | Final,
	All = Primary | Lesser | Final
};

/**
 * 交互选项
 */
UENUM(BlueprintType)
enum class EInteractAction : uint8
{
	// 无
	None = 0 UMETA(DisplayName="无"),
	// 复活
	Revive = 1 UMETA(DisplayName="复活"),
	// 战斗
	Fight = 2 UMETA(DisplayName="战斗"),
	// 对话
	Dialogue = 3 UMETA(DisplayName="对话"),
	// 交易
	Transaction = 4 UMETA(DisplayName="交易"),
	// 拾取
	PickUp = 5 UMETA(DisplayName="拾取"),

	Custom1 = 10,
	Custom2 = 11,
	Custom3 = 12,
	Custom4 = 13,
	Custom5 = 14,
	Custom6 = 15,
	Custom7 = 16,
	Custom8 = 17,
	Custom9 = 18,
	Custom10 = 19
};

/**
 * 交互对象类型
 */
UENUM(BlueprintType)
enum class EInteractAgentType : uint8
{
	// 无
	None = 0 UMETA(DisplayName="无"),
	// 静态的
	Static = 1 UMETA(DisplayName="静态的"),
	// 可移动的
	Movable = 2 UMETA(DisplayName="可移动的")
};

//////////////////////////////////////////////////////////////////////////
// Functions
extern WHFRAMEWORK_API const UObject* GetWorldContext(bool bInEditor = false);

extern WHFRAMEWORK_API UObject* GetMutableWorldContext(bool bInEditor = false);

template<class T = UObject>
extern T* GetDeterminesOutputObject(T* Value, UClass* Class)
{
	if(!Value) return nullptr;
	if(!Class || Value->IsA(Class))
	{
		return Value;
	}
	return nullptr;
}

#define GET_FUNCTION_NAME_THISCLASS(FunctionName) \
	((void)sizeof(&ThisClass::FunctionName), FName(TEXT(#FunctionName)))

//////////////////////////////////////////////////////////////////////////
// Tags
namespace GameplayTags
{
	WHFRAMEWORK_API	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);
	
	////////////////////////////////////////////////////
	// Input_Shortcut
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_InteractSelect);

	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_CameraPanMove);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_CameraRotate);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_CameraZoom);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_CameraSprint);

	////////////////////////////////////////////////////
	// Input_Camera
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_TurnCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_LookUpCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_PanHCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_PanVCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_ZoomCamera);
	
	////////////////////////////////////////////////////
	// Input_Player
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_TurnPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_MoveForwardPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_MoveRightPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_MoveUpPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_JumpPlayer);

	////////////////////////////////////////////////////
	// Input_System
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input_SystemOperation);

	////////////////////////////////////////////////////
	// State_Vitality
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Dead);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Dying);
		
	////////////////////////////////////////////////////
	// State_Pawn
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Pawn_Active);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Pawn_Moving);

	////////////////////////////////////////////////////
	// State_Character
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Falling);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Walking);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Jumping);

	////////////////////////////////////////////////////
	// Event_Hit
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Attack);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Skill);
};
