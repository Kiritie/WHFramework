#include "Voxel/Rendering/VoxelTerrainFrameBudget.h"

#include "HAL/PlatformMisc.h"

int32 FVoxelTerrainFrameBudget::ResolveMaximumWorkers(const FVoxelTerrainFrameBudgetSettings& InSettings)
{
	const int32 LogicalProcessors = FMath::Max(1, FPlatformMisc::NumberOfCoresIncludingHyperthreads());
	const int32 HardwareLimit = FMath::Min(4096, LogicalProcessors);
	if (InSettings.bUseAutomaticWorkerCount)
	{
		return FMath::Clamp(LogicalProcessors - FMath::Max(0, InSettings.ReservedLogicalProcessors), 1, HardwareLimit);
	}
	return FMath::Clamp(InSettings.MaximumWorkers, 1, HardwareLimit);
}

bool FVoxelTerrainFrameBudget::Configure(const FVoxelTerrainFrameBudgetSettings& InSettings, FString& OutError)
{
	if (!FMath::IsFinite(InSettings.TargetFramesPerSecond) || InSettings.TargetFramesPerSecond <= 0.0 ||
		!FMath::IsFinite(InSettings.MaximumTerrainMilliseconds) ||
		!FMath::IsFinite(InSettings.MinimumNearMilliseconds) ||
		!FMath::IsFinite(InSettings.MinimumBackgroundMilliseconds) ||
		!FMath::IsFinite(InSettings.SafetyMilliseconds) || InSettings.SafetyMilliseconds < 0.0 ||
		InSettings.MinimumNearMilliseconds <= 0.0 || InSettings.MinimumBackgroundMilliseconds <= 0.0 ||
		InSettings.MaximumTerrainMilliseconds < InSettings.MinimumNearMilliseconds + InSettings.MinimumBackgroundMilliseconds ||
		!FMath::IsFinite(InSettings.SmoothingAlpha) || InSettings.SmoothingAlpha <= 0.0 || InSettings.SmoothingAlpha > 1.0 ||
		InSettings.ReservedLogicalProcessors < 0 || InSettings.MinimumWorkers < 1 ||
		(!InSettings.bUseAutomaticWorkerCount && InSettings.MaximumWorkers < InSettings.MinimumWorkers))
	{
		OutError = TEXT("动态 Terrain 帧预算不合法");
		return false;
	}
	Settings = InSettings;
	OutError.Reset();
	return true;
}

FVoxelTerrainFrameAllocation FVoxelTerrainFrameBudget::Observe(const FVoxelTerrainFrameObservation& InObservation)
{
	const auto IsValidTime = [](const double InValue)
	{
		return FMath::IsFinite(InValue) && InValue >= 0.0;
	};
	const double Target = 1000.0 / Settings.TargetFramesPerSecond;
	const bool bGameThreadValid = IsValidTime(InObservation.GameThreadMilliseconds);
	const bool bTerrainValid = bGameThreadValid && IsValidTime(InObservation.TerrainGameThreadMilliseconds) &&
		InObservation.TerrainGameThreadMilliseconds <= InObservation.GameThreadMilliseconds;
	const bool bRenderValid = IsValidTime(InObservation.RenderThreadMilliseconds);
	const bool bGPUValid = IsValidTime(InObservation.GPUMilliseconds);
	if (bGameThreadValid)
	{
		LastValidGameThreadMilliseconds = InObservation.GameThreadMilliseconds;
	}
	const double GameThread = bGameThreadValid ? InObservation.GameThreadMilliseconds :
		FMath::Max(LastValidGameThreadMilliseconds, Target) + Settings.SafetyMilliseconds;
	const double NonTerrain = bTerrainValid ? GameThread - InObservation.TerrainGameThreadMilliseconds : GameThread;
	const double Render = bRenderValid ? InObservation.RenderThreadMilliseconds : SmoothedRenderMilliseconds;
	const double GPU = bGPUValid ? InObservation.GPUMilliseconds : SmoothedGPUMilliseconds;
	const auto ObservePressure = [this](const double InPrevious, const double InSample)
	{
		return !bHasObservation || InSample >= InPrevious ? InSample :
			FMath::Lerp(InPrevious, InSample, Settings.SmoothingAlpha);
	};
	SmoothedGameThreadMilliseconds = ObservePressure(SmoothedGameThreadMilliseconds, GameThread);
	SmoothedNonTerrainMilliseconds = ObservePressure(SmoothedNonTerrainMilliseconds, NonTerrain);
	SmoothedRenderMilliseconds = ObservePressure(SmoothedRenderMilliseconds, Render);
	SmoothedGPUMilliseconds = ObservePressure(SmoothedGPUMilliseconds, GPU);
	bHasObservation = true;

	const double Floor = Settings.MinimumNearMilliseconds + Settings.MinimumBackgroundMilliseconds;
	const double Remaining = Target - FMath::Max(NonTerrain, SmoothedNonTerrainMilliseconds) - Settings.SafetyMilliseconds;
	const double RenderPressure = FMath::Max(SmoothedRenderMilliseconds, SmoothedGPUMilliseconds) / Target;
	FVoxelTerrainFrameAllocation Result;
	Result.bInvalidTiming = !bGameThreadValid || !bTerrainValid || !bRenderValid || !bGPUValid;
	Result.bBaseFrameOverloaded = Remaining < Floor || RenderPressure > 1.0;
	// 生成与发布共用配置上限，出生交接后也必须继续推进剩余地形。
	Result.TotalMilliseconds = Settings.MaximumTerrainMilliseconds;
	Result.NearMinimumMilliseconds = Settings.MinimumNearMilliseconds;
	Result.BackgroundMinimumMilliseconds = Settings.MinimumBackgroundMilliseconds;
	const double Span = Settings.MaximumTerrainMilliseconds - Floor;
	Result.BackgroundScale = Span > 0.0 ? FMath::Clamp((Result.TotalMilliseconds - Floor) / Span, 0.0, 1.0) : 0.0;
	Result.ProxyScale = FMath::Clamp(Result.BackgroundScale / 0.35, 0.0, 1.0);
	Result.SurfaceScale = FMath::Clamp((Result.BackgroundScale - 0.20) / 0.55, 0.0, 1.0);
	Result.MacroScale = FMath::Clamp((Result.BackgroundScale - 0.50) / 0.50, 0.0, 1.0);
	// Worker 并发独立于渲染发布预算，避免 GPU 压力把后台数据生成也压成串行。
	const double WorkerScale = InObservation.bInitialSpawn ? 1.0 :
		FMath::Clamp(2.0 - FMath::Max(NonTerrain, SmoothedNonTerrainMilliseconds) / Target, 0.0, 1.0);
	const int32 MaximumWorkers = ResolveMaximumWorkers(Settings);
	const int32 MinimumWorkers = FMath::Min(Settings.MinimumWorkers, MaximumWorkers);
	Result.MaximumWorkers = FMath::Clamp(FMath::RoundToInt(FMath::Lerp(double(MinimumWorkers),
		double(MaximumWorkers), WorkerScale)), MinimumWorkers, MaximumWorkers);
	return Result;
}

void FVoxelTerrainFrameBudget::Reset()
{
	LastValidGameThreadMilliseconds = 0.0;
	SmoothedGameThreadMilliseconds = 0.0;
	SmoothedNonTerrainMilliseconds = 0.0;
	SmoothedRenderMilliseconds = 0.0;
	SmoothedGPUMilliseconds = 0.0;
	bHasObservation = false;
}
