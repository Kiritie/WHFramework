#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Common/CommonModuleStatics.h"
#include "Main/MainModule.h"
#include "Parameter/ParameterModule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMainModulePIEWorldContextTest,
	"WHFramework.Main.PIEWorldContextIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMainModulePIEWorldContextTest::RunTest(const FString&)
{
	UWorld* Worlds[4] = {};
	AMainModule* Owners[4] = {};
	UParameterModule* Modules[4] = {};
	const EWorldType::Type Types[] = {EWorldType::EditorPreview, EWorldType::PIE, EWorldType::PIE, EWorldType::Game};
	ON_SCOPE_EXIT
	{
		for (int32 Index = 3; Index >= 0; --Index)
		{
#if WITH_EDITOR
			if (Modules[Index]) Modules[Index]->OnDestroy();
#endif
			if (Worlds[Index])
			{
				Worlds[Index]->DestroyWorld(false);
				GEngine->DestroyWorldContext(Worlds[Index]);
			}
		}
	};
	for (int32 Index = 0; Index < 4; ++Index)
	{
		Worlds[Index] = UWorld::CreateWorld(Types[Index], false);
		if (!TestNotNull(TEXT("Creates an independent actual world"), Worlds[Index])) return false;
		GEngine->CreateNewWorldContext(Types[Index]).SetCurrentWorld(Worlds[Index]);
		Owners[Index] = Worlds[Index]->SpawnActor<AMainModule>();
		if (!TestNotNull(TEXT("Creates the world module owner"), Owners[Index])) return false;
		Modules[Index] = NewObject<UParameterModule>(Owners[Index]);
		Owners[Index]->GetModules().Add(Modules[Index]);
		Owners[Index]->GetModuleMap().Add(Modules[Index]->GetModuleName(), Modules[Index]);
	}
	TestTrue(TEXT("UE classifies PIE as both editor and game world"), Worlds[1]->IsEditorWorld() && Worlds[1]->IsGameWorld());
	for (const int32 Index : {0, 1, 2, 0, 3, 2, 1, 0})
	{
		TestTrue(TEXT("Main lookup returns the explicit world owner"), AMainModule::GetPtr(Worlds[Index]) == Owners[Index]);
		TestTrue(TEXT("Typed module lookup preserves the explicit world"), UParameterModule::GetPtr(Worlds[Index]) == Modules[Index]);
		TestEqual(TEXT("Only the non-game editor owner takes editor refresh"), Owners[Index]->IsInEditor(), Index == 0);
		const FScopedWorldContext Scope(Worlds[Index]);
		TestTrue(TEXT("Editor resolution never selects a PIE instance"), !UCommonModuleStatics::ResolveCurrentWorld(true)->IsGameWorld());
		if (Index != 0)
		{
			TestTrue(TEXT("Runtime resolution uses the current PIE or game scope"), UCommonModuleStatics::ResolveCurrentWorld() == Worlds[Index]);
			TestTrue(TEXT("Editor queries cannot change the PIE refresh classification"), !Owners[Index]->IsInEditor());
		}
	}
	return true;
}

#endif
