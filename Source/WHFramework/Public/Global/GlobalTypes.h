// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "GlobalTypes.generated.h"

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
