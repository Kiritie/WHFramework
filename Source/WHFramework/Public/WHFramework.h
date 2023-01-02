// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Global/GlobalTypes.h"
#include "Global/GlobalBPLibrary.h"
#include "Modules/ModuleManager.h"

class FWHFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
