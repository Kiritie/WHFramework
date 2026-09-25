#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Scene/SceneModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSceneDeferredMarkerTrackingTest,
	"WHFramework.Scene.Map.DeferredMarkerTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSceneDeferredMarkerTrackingTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	USceneModule* Scene = NewObject<USceneModule>();
	const FGuid PoiId(17, 29, 31, 43);
	FSceneModuleSaveData SaveData;
	SaveData.TrackedMarkerID = PoiId;
	Scene->LoadSaveData(FParameter(SaveData), EPhase::Primary);
	TestEqual(TEXT("Tracking survives before derived marker registration"),
		Scene->GetTrackedMarker(), PoiId);

	FSceneMarker Marker;
	Marker.MarkerID = PoiId;
	Marker.Location = FVector(1000.0, 2000.0, 0.0);
	TestEqual(TEXT("Derived marker registers with stable identity"),
		Scene->AddMarker(Marker), PoiId);
	TestTrue(TEXT("Recreated marker is still tracked"),
		Scene->GetMarkerView(PoiId, FVector::ZeroVector).bTracked);
	const FParameter CapturedParameter = Scene->GetSaveData(true);
	const FSceneModuleSaveData* Captured = CapturedParameter.GetPtr<FSceneModuleSaveData>();
	if (!TestNotNull(TEXT("Scene save data exists"), Captured))
	{
		return false;
	}
	TestEqual(TEXT("Derived marker itself is excluded from save"),
		Captured->Markers.Num(), 0);
	TestEqual(TEXT("Tracked stable identity is saved"),
		Captured->TrackedMarkerID, PoiId);
	Scene->RemoveMarker(PoiId);
	TestFalse(TEXT("Removing the tracked marker clears tracking"),
		Scene->GetTrackedMarker().IsValid());
	Scene->SetWorldMapRange(2000000.f);
	TestEqual(TEXT("World map supports twenty kilometres"),
		Scene->GetWorldMapRange(), 2000000.f);
	return true;
}

#endif
