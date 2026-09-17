#pragma once
#include "Commandlets/Commandlet.h"
#include "VoxelWorldBakeCommandlet.generated.h"
UCLASS()
class WHFRAMEWORKEDITOR_API UVoxelWorldBakeCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVoxelWorldBakeCommandlet();
    virtual int32 Main(const FString& Params) override;
};
