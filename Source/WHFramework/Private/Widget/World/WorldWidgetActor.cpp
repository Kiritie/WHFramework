// Fill out your copyright notice in the Description page of Project Settings.

#include "Widget/World/WorldWidgetActor.h"

#include "WHFrameworkCoreActions.h"
#include "Common/CommonStatics.h"
#include "Widget/World/WorldWidgetComponent.h"

AWorldWidgetActor::AWorldWidgetActor()
{
#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif

	WorldWidget = CreateDefaultSubobject<UWorldWidgetComponent>(FName("WorldWidget"));
	WorldWidget->bAutoCreate = false;
	RootComponent = WorldWidget;
	
	bAutoCreate = true;
	bShowInEditor = false;
}

void AWorldWidgetActor::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AWorldWidgetActor::OnPreparatory_Implementation()
{
	Super::OnPreparatory_Implementation();

	if(bAutoCreate)
	{
		CreateWorldWidget();
	}
}

void AWorldWidgetActor::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AWorldWidgetActor::OnTermination_Implementation()
{
	Super::OnTermination_Implementation();

	DestroyWorldWidget(true);
}

void AWorldWidgetActor::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
		FWHFrameworkCoreDelegates::OnBlueprintCompiled.AddUObject(this, &AWorldWidgetActor::OnRefreshWorldWidget);

		// FWHFrameworkCoreDelegates::OnActiveViewportChanged.AddUObject(this, &AWorldWidgetActor::OnRefreshWorldWidget);
		
		FWHFrameworkCoreDelegates::OnViewportListChanged.AddUObject(this, &AWorldWidgetActor::OnRefreshWorldWidget);
	}
#endif
}

void AWorldWidgetActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if(!GIsPlaying && UCommonStatics::GetWorldContext(true))
	{
		if(bShowInEditor)
		{
			CreateWorldWidget(true);
		}
		else
		{
			DestroyWorldWidget(false, true);
		}
	}
}

void AWorldWidgetActor::Destroyed()
{
	Super::Destroyed();

	if(!GIsPlaying && UCommonStatics::GetWorldContext(true) && bShowInEditor)
	{
		DestroyWorldWidget(false, true);
	}
}

#if WITH_EDITOR
void AWorldWidgetActor::OnRefreshWorldWidget()
{
	if(!GIsPlaying && UCommonStatics::GetWorldContext(true) && bShowInEditor)
	{
		CreateWorldWidget(true);
	}
}
#endif

void AWorldWidgetActor::CreateWorldWidget(bool bInEditor)
{
	WorldWidget->CreateWorldWidget(GetWidgetParams(), bInEditor);
}

void AWorldWidgetActor::DestroyWorldWidget(bool bRecovery, bool bInEditor)
{
	WorldWidget->DestroyWorldWidget(bRecovery, bInEditor);
}

TArray<FParameter> AWorldWidgetActor::GetWidgetParams_Implementation() const
{
	return TArray<FParameter>();
}

