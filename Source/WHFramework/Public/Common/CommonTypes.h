// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

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
	OnlyTime,
	OnlyModules
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

struct FSaveDataArchive : public FObjectAndNameAsStringProxyArchive
{
	FSaveDataArchive(FArchive& InInnerArchive) : FObjectAndNameAsStringProxyArchive(InInnerArchive, false)
	{
		ArIsSaveGame = true;
	}
};

//////////////////////////////////////////////////////////////////////////
// Functions
extern UObject* GetWorldContext(bool bInEditor = false);

//////////////////////////////////////////////////////////////////////////
// MACROS
#define PHASEC(A, B) \
((uint8)A & (uint8)B) != 0

#define DON(Count, Expression) \
for(int32 _Index = 0; _Index < Count; _Index++) \
{ \
	Expression \
}

#define DON_WITHINDEX(Count, Index, Expression) \
for(int32 Index = 0; Index < Count; Index++) \
{ \
	Expression \
}

#define ITER_ARRAY(Array, Item, Expression) \
for(auto& Item : Array) \
{ \
	Expression \
}

#define ITER_ARRAY_WITHINDEX(Array, Index, Item, Expression) \
int32 Index = 0; \
for(auto& Item : Array) \
{ \
	Expression \
	Index++; \
}

#define ITER_MAP(Map, Item, Expression) \
for(auto& Item : Map) \
{ \
	Expression \
}

#define ITER_MAP_WITHINDEX(Map, Index, Item, Expression) \
int32 Index = 0; \
for(auto& Item : Map) \
{ \
	Expression \
	Index++; \
}

namespace GameplayTags
{
	WHFRAMEWORK_API	FGameplayTag FindTagByString(const FString& TagString, bool bMatchPartialString = false);

	////////////////////////////////////////////////////
	// Input_Camera
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_TurnCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_LookUpCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_PanHCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_PanVCamera);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_ZoomCamera);
	
	////////////////////////////////////////////////////
	// Input_Player
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_TurnPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveHPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveVPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveForwardPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveRightPlayer);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_MoveUpPlayer);
	
	////////////////////////////////////////////////////
	// State_Vitality
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateTag_Vitality_Dead);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateTag_Vitality_Dying);
	
	////////////////////////////////////////////////////
	// State_Character
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateTag_Character_Active);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateTag_Character_Falling);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateTag_Character_Walking);
	WHFRAMEWORK_API	UE_DECLARE_GAMEPLAY_TAG_EXTERN(StateTag_Character_Jumping);
};
