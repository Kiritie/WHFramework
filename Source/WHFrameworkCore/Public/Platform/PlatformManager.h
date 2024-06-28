
#pragma once
#include "SceneTypes.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class WHFRAMEWORKCORE_API FPlatformManager : public FManagerBase
{
	GENERATED_MANAGER(FPlatformManager)

public:
	// ParamSets default values for this actor's properties
	FPlatformManager();

	virtual ~FPlatformManager() override;

	static const FUniqueType Type;

public:
	virtual void OnInitialize() override;

	virtual void OnTermination() override;
};
