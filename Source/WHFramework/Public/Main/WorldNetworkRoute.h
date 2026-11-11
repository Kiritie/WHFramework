#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldNetworkRoute.generated.h"

UCLASS(NotPlaceable, Transient)
class WHFRAMEWORK_API AWorldNetworkRouteAuthority : public AActor
{
	GENERATED_BODY()

public:
	AWorldNetworkRouteAuthority();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGuid GetEpoch() const { return WorldEpoch; }

private:
	UPROPERTY(Replicated)
	FGuid WorldEpoch;
};

class UWorld;

namespace WorldNetworkRoute
{
	WHFRAMEWORK_API FGuid GetEpoch(UWorld* InWorld);
}
