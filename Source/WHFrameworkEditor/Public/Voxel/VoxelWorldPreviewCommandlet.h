#pragma once

#include "Commandlets/Commandlet.h"
#include "VoxelWorldPreviewCommandlet.generated.h"

/** Exports natural terrain, landform and water samples for large-area visual inspection. */
UCLASS()
class WHFRAMEWORKEDITOR_API UVoxelWorldPreviewCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UVoxelWorldPreviewCommandlet();

	virtual int32 Main(const FString& Params) override;
};
