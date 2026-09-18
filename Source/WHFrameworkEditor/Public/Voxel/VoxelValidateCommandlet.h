#pragma once
#include "Commandlets/Commandlet.h"
#include "VoxelValidateCommandlet.generated.h"
UCLASS()
class WHFRAMEWORKEDITOR_API UVoxelValidateCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVoxelValidateCommandlet();
    virtual int32 Main(const FString& Params) override;
};
