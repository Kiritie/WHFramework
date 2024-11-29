// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
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
	Passivity = 1 UMETA(DisplayName="被动的"),
	// 可移动的
	Initiative = 2 UMETA(DisplayName="主动的")
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FLanguageType
{
	GENERATED_BODY()

public:
	FORCEINLINE FLanguageType()
	{
		DisplayName = TEXT("");
		LocalCulture = TEXT("");
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString LocalCulture;
};

//////////////////////////////////////////////////////////////////////////
// Property
#define GET_MEMBER_PROPERTY(ClassName, PropertyName) \
	FindFieldChecked<FProperty>(ClassName::StaticClass(), GET_MEMBER_NAME_CHECKED(ClassName, PropertyName)) \

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
	GET_FUNCTION_NAME_CHECKED(ThisClass, FunctionName)

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
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Active);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Dead);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Dying);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Walking);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Vitality_Interrupting);

	////////////////////////////////////////////////////
	// State_Pawn
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Pawn_Moving);

	////////////////////////////////////////////////////
	// State_Character
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_FreeToAnim);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Animating);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Falling);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Jumping);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Crouching);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Swimming);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Floating);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Climbing);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Character_Flying);

	////////////////////////////////////////////////////
	// Ability_Vitality_Action
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Vitality_Action_Death);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Vitality_Action_Spawn);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Vitality_Action_Static);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Vitality_Action_Walk);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Vitality_Action_Interrupt);

	////////////////////////////////////////////////////
	// Ability_Character_Action
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Jump);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Fall);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Crouch);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Swim);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Float);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Climb);
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Character_Action_Fly);

	////////////////////////////////////////////////////
	// Event_Hit
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Attack);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Skill);
};
