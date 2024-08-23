// Fill out your copyright notice in the Description page of Project Settings.

#include "Event/Handle/Camera/EventHandle_SetCameraView.h"

UEventHandle_SetCameraView::UEventHandle_SetCameraView()
{
	CameraViewData = FCameraViewData();
	bCacheData = true;

#if WITH_EDITORONLY_DATA
	bRefreshData = false;
#endif
}

void UEventHandle_SetCameraView::OnDespawn_Implementation(bool bRecovery)
{
	CameraViewData = FCameraViewData();
	bCacheData = true;
}

void UEventHandle_SetCameraView::Parse_Implementation(const TArray<FParameter>& InParams)
{
	CameraViewData.FromParams(InParams);
	if(InParams.IsValidIndex(InParams.Num() - 1))
	{
		bCacheData = InParams[InParams.Num() - 1];
	}
}

TArray<FParameter> UEventHandle_SetCameraView::Pack_Implementation()
{
	TArray<FParameter> ReturnValues = CameraViewData.ToParams();
	ReturnValues.Add(bCacheData);
	return ReturnValues;
}

#if WITH_EDITOR
void UEventHandle_SetCameraView::GetCameraView()
{
	CameraViewData.CameraViewParams.GetCameraParams(CameraViewData.CameraViewTarget.LoadSynchronous());

	Modify();
}

void UEventHandle_SetCameraView::SetCameraView(const FCameraParams& InCameraParams)
{
	CameraViewData.CameraViewParams.SetCameraParams(InCameraParams, CameraViewData.CameraViewTarget.LoadSynchronous());

	Modify();
}

void UEventHandle_SetCameraView::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FProperty* Property = PropertyChangedEvent.MemberProperty;

	if(Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = Property->GetFName();

		if(PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UEventHandle_SetCameraView, bRefreshData))
		{
			if(bRefreshData)
			{
				GetCameraView();
				bRefreshData = false;
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
