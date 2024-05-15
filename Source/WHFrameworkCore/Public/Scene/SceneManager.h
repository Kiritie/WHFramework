// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SceneTypes.h"
#include "WHFrameworkCoreTypes.h"
#include "Main/MainTypes.h"
#include "Main/Base/ManagerBase.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class WHFRAMEWORKCORE_API FSceneManager : public FManagerBase
{
	GENERATED_MANAGER(FSceneManager)

public:
	// ParamSets default values for this actor's properties
	FSceneManager();
	
	FSceneManager(FUniqueType InType);

	virtual ~FSceneManager() override;

	static const FUniqueType Type;

	//////////////////////////////////////////////////////////////////////////
	// World
public:
#if WITH_EDITOR
	virtual void SelectActorsInCurrentWorld(const TArray<AActor*>& InActors, bool bForce = false) { }
	
	virtual void DeselectActorsInCurrentWorld(const TArray<AActor*>& InActors) { }
	
	virtual void DeselectAllActorInCurrentWorld() { }

	virtual TArray<AActor*> GetSelectedActorsInCurrentWorld() const { return TArray<AActor*>(); }
#endif

	//////////////////////////////////////////////////////////////////////////
	// Viewport
public:
#if WITH_EDITOR
	virtual SConstraintCanvas::FSlot* AddWidgetToActiveViewport(const TSharedRef<SWidget>& InWidget) { return nullptr; }

	virtual TArray<SConstraintCanvas::FSlot*> AddWidgetToAllViewport(const TSharedRef<SWidget>& InWidget) { return TArray<SConstraintCanvas::FSlot*>(); }

	virtual void RemoveWidgetFromActiveViewport(const TSharedRef<SWidget>& InWidget) { }

	virtual void RemoveWidgetFromAllViewport(const TSharedRef<SWidget>& InWidget) { }
#endif
};
