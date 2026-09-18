#pragma once
#include "UObject/Interface.h"
#include "VoxelAgentInterface.generated.h"
class UVoxelAgentComponent;
UINTERFACE(BlueprintType,meta=(CannotImplementInterfaceInBlueprint))
class WHFRAMEWORK_API UVoxelAgentInterface:public UInterface
{
    GENERATED_BODY()
};
class WHFRAMEWORK_API IVoxelAgentInterface
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Voxel")
    virtual UVoxelAgentComponent* GetVoxelAgentComponent()const=0;
};
