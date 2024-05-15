// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/Layout/SConstraintCanvas.h"

//////////////////////////////////////////////////////////////////////////
// Delegates
class WHFRAMEWORKCORE_API FWHFrameworkCoreDelegates
{
public:
	static TMulticastDelegate<void()> OnBlueprintCompiled;

	static TMulticastDelegate<void()> OnActiveViewportChanged;

	static TMulticastDelegate<void()> OnViewportListChanged;
};
