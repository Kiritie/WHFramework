#pragma once
#include "Commandlets/Commandlet.h"
#include "VoxelLegacyExportCommandlet.generated.h"
UCLASS()
class WHFRAMEWORKEDITOR_API UVoxelLegacyExportCommandlet:public UCommandlet
{
    GENERATED_BODY()
public:
    UVoxelLegacyExportCommandlet();
    virtual int32 Main(const FString& Params)override;
};
