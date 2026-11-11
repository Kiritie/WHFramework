#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ScenePlayerIdentityInterface.generated.h"

/** Supplies the durable player identity used by player-owned scene selections. */
UINTERFACE(BlueprintType)
class WHFRAMEWORK_API UScenePlayerIdentityInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API IScenePlayerIdentityInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Scene")
	FGuid GetSceneMarkerPlayerId() const;
};
