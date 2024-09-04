// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"

class WHFRAMEWORKCORE_API FApplicationManager : public FManagerBase
{
	GENERATED_MANAGER(FApplicationManager)

public:
	FApplicationManager();
	
	static FUniqueType Type;

public:
	virtual void OnInitialize() override;
};
