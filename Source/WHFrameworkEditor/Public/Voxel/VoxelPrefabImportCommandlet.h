#pragma once
#include "Commandlets/Commandlet.h"
#include "VoxelPrefabImportCommandlet.generated.h"
UCLASS()
class WHFRAMEWORKEDITOR_API UVoxelPrefabImportCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UVoxelPrefabImportCommandlet();
    virtual int32 Main(const FString& Params) override;
};
