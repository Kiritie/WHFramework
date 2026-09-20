#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Voxel/Runtime/VoxelChangeIndex.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelChangeIndexSparseEnumerateTest,
	"WHFramework.Voxel.Runtime.ChangeIndexSparseEnumerate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelChangeIndexSparseEnumerateTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelChangeIndex Index;
	const TArray<FIntVector> Modified =
	{
		FIntVector(-17, -9, -3),
		FIntVector(-1, 0, 1),
		FIntVector(0, 0, 0),
		FIntVector(4000000, -3000000, 2000000)
	};
	for (const FIntVector& Section : Modified)
	{
		Index.SetModified(Section, true);
	}

	TArray<FIntVector> Sections;
	TestTrue(
		TEXT("Sparse enumeration completes across extreme bounds"),
		Index.Enumerate(
			{ FIntVector(MIN_int32), FIntVector(MAX_int32) },
			Sections));
	TestEqual(TEXT("Only modified sections are returned"), Sections.Num(), Modified.Num());
	for (const FIntVector& Section : Modified)
	{
		TestTrue(TEXT("Modified section is returned"), Sections.Contains(Section));
	}
	for (int32 IndexValue = 1; IndexValue < Sections.Num(); ++IndexValue)
	{
		const FIntVector& Previous = Sections[IndexValue - 1];
		const FIntVector& Current = Sections[IndexValue];
		TestTrue(
			TEXT("Sparse enumeration is ordered by Z, then Y, then X"),
			Previous.Z < Current.Z ||
				(Previous.Z == Current.Z && Previous.Y < Current.Y) ||
				(Previous.Z == Current.Z && Previous.Y == Current.Y && Previous.X < Current.X));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FVoxelChangeIndexCancellationTest,
	"WHFramework.Voxel.Runtime.ChangeIndexCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FVoxelChangeIndexCancellationTest::RunTest(const FString& InParameters)
{
	(void)InParameters;
	FVoxelChangeIndex Index;
	for (int32 Value = -512; Value <= 512; ++Value)
	{
		Index.SetModified(FIntVector(Value * 8, Value, -Value), true);
	}

	TAtomic<bool> Cancel(true);
	TArray<FIntVector> Sections = { FIntVector::ZeroValue };
	TestFalse(
		TEXT("Canceled sparse enumeration reports cancellation"),
		Index.Enumerate(
			{ FIntVector(MIN_int32), FIntVector(MAX_int32) },
			Sections,
			&Cancel));
	TestTrue(TEXT("Canceled enumeration does not publish partial results"), Sections.IsEmpty());
	return true;
}

#endif
