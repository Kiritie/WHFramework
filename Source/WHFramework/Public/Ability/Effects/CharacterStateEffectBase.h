#pragma once
#include "Ability/Effects/EffectBase.h"
#include "NativeGameplayTags.h"
#include "CharacterStateEffectBase.generated.h"

namespace CharacterLifecycleTags
{
	WHFRAMEWORK_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Suspended);
}

UCLASS()
class WHFRAMEWORK_API UCharacterStateEffectBase : public UEffectBase
{
	GENERATED_BODY()
public:
	UCharacterStateEffectBase();
};

UCLASS()
class WHFRAMEWORK_API UCharacterTimedStateEffectBase : public UEffectBase
{
	GENERATED_BODY()
public:
	UCharacterTimedStateEffectBase();
};
