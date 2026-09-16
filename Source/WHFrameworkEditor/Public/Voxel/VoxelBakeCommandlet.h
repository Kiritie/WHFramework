#pragma once
#include "Commandlets/Commandlet.h"
#include "VoxelBakeCommandlet.generated.h"
UCLASS()
class WHFRAMEWORKEDITOR_API UVoxelBakeCommandlet : public UCommandlet
{
	GENERATED_BODY()
public:
	UVoxelBakeCommandlet();
	virtual int32 Main(const FString& Params) override;
};
