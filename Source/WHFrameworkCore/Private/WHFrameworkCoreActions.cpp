// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFrameworkCoreActions.h"

//////////////////////////////////////////////////////////////////////////
// Delegates
TMulticastDelegate<void()> FWHFrameworkCoreDelegates::OnBlueprintCompiled;

TMulticastDelegate<void()> FWHFrameworkCoreDelegates::OnActiveViewportChanged;

TMulticastDelegate<void()> FWHFrameworkCoreDelegates::OnViewportListChanged;
