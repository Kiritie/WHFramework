// Copyright Epic Games, Inc. All Rights Reserved.

#include "Scene/SceneEditor.h"
#include "Type/NativeTypeIdentity.h"

#include "Editor.h"
#include "IAssetViewport.h"
#include "LevelEditor.h"
#include "SceneViewExtension.h"
#include "Selection.h"
#include "SLevelViewport.h"
#include "WHFrameworkCoreActions.h"
#include "Main/MainManager.h"

#define LOCTEXT_NAMESPACE "FSceneEditor"

IMPLEMENTATION_EDITOR_MODULE(FSceneEditorModule)

FSceneEditorModule::FSceneEditorModule()
{
	AppIdentifier = FName("SceneEditorApp");
}

void FSceneEditorModule::StartupModule()
{
	FSceneEditorManager::Register(true);
}

void FSceneEditorModule::ShutdownModule()
{
	FSceneEditorManager::UnRegister();
}

void FSceneEditorModule::RegisterSettings(ISettingsModule* SettingsModule)
{
	
}

void FSceneEditorModule::UnRegisterSettings(ISettingsModule* SettingsModule)
{

}

bool FSceneEditorModule::HandleSettingsSaved()
{
	return true;
}

void FSceneEditorModule::RegisterCommands(const TSharedPtr<FUICommandList>& Commands)
{

}

void FSceneEditorModule::RegisterMenus(const TSharedPtr<FUICommandList>& Commands)
{

}

void FSceneEditorModule::RegisterAssetTypeAction(IAssetTools& AssetTools, EAssetTypeCategories::Type AssetCategory, TArray<TSharedPtr<IAssetTypeActions>>& AssetTypeActions)
{

}

void FSceneEditorModule::RegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
}

void FSceneEditorModule::UnRegisterCustomization(FPropertyEditorModule& PropertyEditor)
{
}

//////////////////////////////////////////////////////////////////////////
// SceneEditorManager

const FUniqueType FSceneEditorManager::Type = FSceneManager::Type;

IMPLEMENTATION_MANAGER(FSceneEditorManager)

FSceneEditorManager::FSceneEditorManager() : FSceneManager(Type)
{
}

FSceneEditorManager::~FSceneEditorManager()
{
}

void FSceneEditorManager::OnInitialize()
{
	FSceneManager::OnInitialize();

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	
	LevelEditorCreatedHandle = LevelEditorModule.OnLevelEditorCreated().AddRaw(this, &FSceneEditorManager::OnLevelEditorCreated);
	if (TSharedPtr<ILevelEditor> Existing = LevelEditorModule.GetLevelEditorInstance().Pin()) OnLevelEditorCreated(Existing);
}

void FSceneEditorManager::OnTermination()
{
    if (FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor"))
    {
        if (LevelEditorCreatedHandle.IsValid()) LevelEditor->OnLevelEditorCreated().Remove(LevelEditorCreatedHandle);
    }
    LevelEditorCreatedHandle.Reset();
    for (const FEditorViewportBinding& Binding : EditorViewportBindings)
    {
        if (TSharedPtr<ILevelEditor> Editor = Binding.Editor.Pin()) Editor->OnActiveViewportChanged().Remove(Binding.Handle);
    }
    EditorViewportBindings.Reset();
    for (const auto& Pair : ViewportRootCanvases)
    {
        if (TSharedPtr<IAssetViewport> Viewport = Pair.Viewport.Pin(); Viewport && Pair.Canvas) Viewport->RemoveOverlayWidget(Pair.Canvas.ToSharedRef());
    }
    ViewportRootCanvases.Reset();
    FSceneManager::OnTermination();
}

void FSceneEditorManager::OnLevelEditorCreated(TSharedPtr<ILevelEditor> InLevelEditor)
{
    if (!InLevelEditor) return;
    for (const FEditorViewportBinding& Binding : EditorViewportBindings) if (Binding.Editor.Pin() == InLevelEditor) return;
    FEditorViewportBinding& Binding = EditorViewportBindings.AddDefaulted_GetRef();
    Binding.Editor = InLevelEditor;
    Binding.Handle = InLevelEditor->OnActiveViewportChanged().AddRaw(this, &FSceneEditorManager::OnActiveViewportChanged);
}

void FSceneEditorManager::OnActiveViewportChanged(TSharedPtr<IAssetViewport> OldViewport, TSharedPtr<IAssetViewport> NewViewport)
{
	ViewportRootCanvases.RemoveAll([](const FViewportCanvasBinding& Binding) { return !Binding.Viewport.IsValid(); });
	if(FWHFrameworkCoreDelegates::OnActiveViewportChanged.IsBound())
	{
		FWHFrameworkCoreDelegates::OnActiveViewportChanged.Broadcast();
	}
}

bool FSceneEditorManager::GetSceneViewProjectionData(const FEditorViewportClient* InViewport, FSceneViewProjectionData& OutProjectionData)
{
	FViewport* Viewport = InViewport->Viewport;

	if ((Viewport == NULL) || (Viewport->GetSizeXY().X == 0) || (Viewport->GetSizeXY().Y == 0))
	{
		return false;
	}

	const FVector2D Origin = FVector2D::Zero();

	int32 X = FMath::TruncToInt(Origin.X * Viewport->GetSizeXY().X);
	int32 Y = FMath::TruncToInt(Origin.Y * Viewport->GetSizeXY().Y);

	X += Viewport->GetInitialPositionXY().X;
	Y += Viewport->GetInitialPositionXY().Y;

	uint32 SizeX = FMath::TruncToInt(1.f * Viewport->GetSizeXY().X);
	uint32 SizeY = FMath::TruncToInt(1.f * Viewport->GetSizeXY().Y);

	const FIntRect UnconstrainedRectangle = FIntRect(X, Y, X + SizeX, Y + SizeY);

	OutProjectionData.SetViewRectangle(UnconstrainedRectangle);

	FMinimalViewInfo ViewInfo;
	ViewInfo.Location = InViewport->GetViewLocation();
	ViewInfo.Rotation = InViewport->GetViewRotation();
	ViewInfo.FOV = InViewport->ViewFOV;
	ViewInfo.DesiredFOV = ViewInfo.FOV;

	OutProjectionData.ViewOrigin = ViewInfo.Location;
	OutProjectionData.ViewRotationMatrix = FInverseRotationMatrix(ViewInfo.Rotation) *
		FMatrix
		(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1)
		);

	FMinimalViewInfo::CalculateProjectionMatrixGivenView(ViewInfo, AspectRatio_MajorAxisFOV, Viewport, OutProjectionData);
	for (auto& ViewExt : GEngine->ViewExtensions->GatherActiveExtensions(FSceneViewExtensionContext(Viewport)))
	{
		ViewExt->SetupViewProjectionMatrix(OutProjectionData);
	}

	return true;
}

void FSceneEditorManager::SelectActorsInCurrentWorld(const TArray<AActor*>& InActors, bool bForce)
{
	if(bForce) DeselectAllActorInCurrentWorld();
	
	for(auto Iter : InActors)
	{
		GEditor->SelectActor(Iter, true, true);
	}
}

void FSceneEditorManager::DeselectActorsInCurrentWorld(const TArray<AActor*>& InActors)
{
	for(auto Iter : InActors)
	{
		GEditor->SelectActor(Iter, false, true);
	}
}

void FSceneEditorManager::DeselectAllActorInCurrentWorld()
{
	GEditor->GetSelectedActors()->DeselectAll();
}

TArray<AActor*> FSceneEditorManager::GetSelectedActorsInCurrentWorld() const
{
	TArray<AActor*> ReturnValues;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(ReturnValues);
	return ReturnValues;
}

FVector FSceneEditorManager::GetActiveViewportViewLocation() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<IAssetViewport> Viewport = LevelEditorModule.GetFirstActiveViewport();

	return Viewport->GetAssetViewportClient().GetViewLocation();
}

FRotator FSceneEditorManager::GetActiveViewportViewRotation() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<IAssetViewport> Viewport = LevelEditorModule.GetFirstActiveViewport();

	return Viewport->GetAssetViewportClient().GetViewRotation();
}

SConstraintCanvas::FSlot* FSceneEditorManager::AddWidgetToViewport(const TSharedPtr<IAssetViewport>& InViewport, const TSharedRef<SWidget>& InWidget)
{
	SConstraintCanvas::FSlot* Slot = nullptr;

	if (InViewport.IsValid())
	{
		GetOrCreateViewportRootCanvas(InViewport)
		->AddSlot()
		.Expose(Slot)
		.AutoSize(true)
		[
			InWidget
		];
	}
	
	return Slot;
}

SConstraintCanvas::FSlot* FSceneEditorManager::AddWidgetToActiveViewport(const TSharedRef<SWidget>& InWidget)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<IAssetViewport> Viewport = LevelEditorModule.GetFirstActiveViewport();

	return AddWidgetToViewport(Viewport, InWidget);
}

TArray<SConstraintCanvas::FSlot*> FSceneEditorManager::AddWidgetToAllViewport(const TSharedRef<SWidget>& InWidget)
{
	TArray<SConstraintCanvas::FSlot*> Slots;

	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance().Pin();

	if(LevelEditor.IsValid())
	{
		for(auto Iter : LevelEditor->GetViewports())
		{
			Slots.Add(AddWidgetToViewport(Iter, InWidget));
		}
	}

	return Slots;
}

void FSceneEditorManager::RemoveWidgetFromViewport(const TSharedPtr<IAssetViewport>& InViewport, const TSharedRef<SWidget>& InWidget)
{
	if (InViewport.IsValid())
	{
		for (const FViewportCanvasBinding& Binding : ViewportRootCanvases)
        {
            if (Binding.Viewport.Pin() == InViewport && Binding.Canvas)
            {
                Binding.Canvas->RemoveSlot(InWidget);
                break;
            }
        }
	}
}

void FSceneEditorManager::RemoveWidgetFromActiveViewport(const TSharedRef<SWidget>& InWidget)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	const TSharedPtr<IAssetViewport> Viewport = LevelEditorModule.GetFirstActiveViewport();

	RemoveWidgetFromViewport(Viewport, InWidget);
}

void FSceneEditorManager::RemoveWidgetFromAllViewport(const TSharedRef<SWidget>& InWidget)
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::Get().GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<ILevelEditor> LevelEditor = LevelEditorModule.GetLevelEditorInstance().Pin();

	if(LevelEditor.IsValid())
	{
		for(auto Iter : LevelEditor->GetViewports())
		{
			RemoveWidgetFromViewport(Iter, InWidget);
		}
	}
}

TSharedRef<SViewportRootCanvas> FSceneEditorManager::GetOrCreateViewportRootCanvas(const TSharedPtr<IAssetViewport>& InViewport)
{
    check(InViewport.IsValid());
    ViewportRootCanvases.RemoveAll([](const FViewportCanvasBinding& Binding) { return !Binding.Viewport.IsValid(); });
    for (const FViewportCanvasBinding& Binding : ViewportRootCanvases)
    {
        if (Binding.Viewport.Pin() == InViewport && Binding.Canvas) return Binding.Canvas.ToSharedRef();
    }
    TSharedRef<SViewportRootCanvas> Canvas = SNew(SViewportRootCanvas).Viewport(InViewport);
    InViewport->AddOverlayWidget(Canvas);
    FViewportCanvasBinding& Binding = ViewportRootCanvases.AddDefaulted_GetRef();
    Binding.Viewport = InViewport;
    Binding.Canvas = Canvas;
    return Canvas;
}

#undef LOCTEXT_NAMESPACE
