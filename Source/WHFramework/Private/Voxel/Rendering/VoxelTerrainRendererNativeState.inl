namespace
{
	void FVoxelRendererSectionChanged::operator()(const FIntVector& InSection) const
	{
		if (auto* Owner = Module.Get()) Owner->NotifyTerrainSectionReplacement(InSection, Epoch);
	}

	void FVoxelRendererDeleteState::operator()() const
	{
		delete State;
	}

	void FVoxelRendererStateDeleter::operator()(FVoxelTerrainRendererState* InState) const
	{
		if (IsInGameThread()) delete InState;
		else AsyncTask(ENamedThreads::GameThread, TUniqueFunction<void()>(FVoxelRendererDeleteState{Lease, InState}));
	}

	uint64 VoxelRendererStateInputBytes(const bool bInRendering)
	{
		using FController = SharedPointerInternals::TReferenceControllerWithDeleter<FVoxelTerrainRendererState,
			FVoxelRendererStateDeleter, ESPMode::ThreadSafe>;
		uint64 Bytes = FMemory::QuantizeSize(sizeof(FVoxelTerrainRendererState), alignof(FVoxelTerrainRendererState)) +
			FMemory::QuantizeSize(sizeof(FController), alignof(FController)) +
			VoxelBoundedAllocation::FunctionBytes<FVoxelRendererSectionChanged, false>() +
			VoxelBoundedAllocation::FunctionBytes<FVoxelRendererDeleteState>() +
			VoxelBoundedAllocation::FunctionBytes<FVoxelRendererRetirement>();
		if (bInRendering)
		{
			using FExtensionController = SharedPointerInternals::TReferenceControllerWithDeleter<FVoxelTerrainViewExtension,
				SharedPointerInternals::DefaultDeleter<FVoxelTerrainViewExtension>, ESPMode::ThreadSafe>;
			Bytes += FMemory::QuantizeSize(sizeof(FVoxelTerrainViewExtension), alignof(FVoxelTerrainViewExtension)) +
				FMemory::QuantizeSize(sizeof(FExtensionController), alignof(FExtensionController));
		}
		return Bytes;
	}

	bool FVoxelRendererRetirement::operator()(float InDelta) const
	{
		const double Deadline = FPlatformTime::Seconds() + 0.00025;
		FSnapshotPtr SubmittedSnapshot;
		FFrameSubmissionData Submission;
		for (int32 Count = 0; Count < 16 && FPlatformTime::Seconds() < Deadline &&
			State->Submissions.TryConsume(SubmittedSnapshot, Submission); ++Count)
		{
			State->RegisterFrameSubmission(SubmittedSnapshot, Submission);
			SubmittedSnapshot.Reset();
		}
		const int32 FrameChecks = FMath::Min(32, State->RetiringFrameCount);
		for (int32 Count = 0; Count < FrameChecks && FPlatformTime::Seconds() < Deadline; ++Count)
		{
			const auto* Snapshot = State->NextRetiringFrame();
			if (!Snapshot) break;
			auto& Frame = Snapshot->Retirement;
			if (Frame.bHasReceipts && !Frame.bReceiptsSettled)
			{
				for (int32 Step = 0; Step < 8 && Frame.ReceiptCursor < Snapshot->Issued.Num() &&
					FPlatformTime::Seconds() < Deadline; ++Step)
					State->AdvanceFrameReceipt(Frame, true);
				Frame.bReceiptsSettled = Frame.ReceiptCursor == Snapshot->Issued.Num();
			}
			if ((!Frame.bHasReceipts || Frame.bReceiptsSettled) && Frame.Snapshot.IsUnique() && Frame.RenderFence.IsFenceComplete() &&
				Frame.AreGpuFencesComplete())
			{
				State->RemoveRetiringFrame(*Snapshot);
			}
		}
		// SceneProxy、Family 和 RDG 的引用必须结束，只剩 ticker 捕获时
		// 才能销毁材质保持者及剩余 CPU 所有权镜像。
		if (State->Submissions.IsEmpty() && State->RetiringFrameCount == 0 && State.IsUnique())
		{
			State->ComponentPin.Reset();
			return false;
		}
		return true;
	}
}

EVoxelTerrainResourceAdmission FVoxelTerrainRenderer::GetNativeStateAdmission() const
{
	if (bStopped) return EVoxelTerrainResourceAdmission::InvalidRequest;
	if (State && State->Component.IsValid() && State->ProxyAdmission != EVoxelTerrainResourceAdmission::Granted)
		return State->ProxyAdmission == EVoxelTerrainResourceAdmission::None ?
			EVoxelTerrainResourceAdmission::CapacityBlocked : State->ProxyAdmission;
	return BootstrapAdmission;
}

bool FVoxelTerrainRenderer::EnsureState()
{
	check(IsInGameThread());
	if (bStopped) return false;
	if (State)
	{
		if (!State->bActive.Load()) return false;
		if (State->ProxyAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked && !State->bProxyRecreationPending &&
			RendererHasExternalRelease(BoundResources, BootstrapConsumer, State->ProxyCapacityVersion))
		{
			if (auto* Component = State->Component.Get())
			{
				State->bProxyRecreationPending = true;
				Component->MarkRenderStateDirty();
			}
		}
		return GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::Granted;
	}
	if (BootstrapAdmission == EVoxelTerrainResourceAdmission::CapacityUnachievable ||
		BootstrapAdmission == EVoxelTerrainResourceAdmission::InvalidRequest) return false;
	if (BootstrapAdmission == EVoxelTerrainResourceAdmission::CapacityBlocked &&
		!RendererHasExternalRelease(BoundResources, BootstrapConsumer, BootstrapCapacityVersion)) return false;
	UVoxelModule* Module = BoundModule.Get();
	UWorld* World = BoundWorld.Get();
	if (!Module || !World || Module->GetWorld() != World || Module->GetWorldEpoch() != BootstrapConsumer.WorldEpoch)
	{
		BootstrapAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
		return false;
	}
	const bool bRendering = World->GetNetMode() != NM_DedicatedServer;
	BoundResources->TryGetReleaseVersionExcludingOwner(BootstrapConsumer, BootstrapCapacityVersion);
	TSharedPtr<FVoxelTerrainResourceLease, ESPMode::ThreadSafe> Lease;
	BootstrapAdmission = BoundResources->TryAcquire(BootstrapConsumer, EVoxelTerrainResourcePool::Input,
		VoxelRendererStateInputBytes(bRendering), Lease);
	if (BootstrapAdmission != EVoxelTerrainResourceAdmission::Granted) return false;
	auto Candidate = TSharedPtr<FVoxelTerrainRendererState, ESPMode::ThreadSafe>(
		new FVoxelTerrainRendererState(*Module, *BoundScheduler, BootstrapConsumer.WorldEpoch, BootstrapConsumer, Lease),
		FVoxelRendererStateDeleter{Lease});
	Candidate->Handoff.SetSectionChangedCallback(FVoxelRendererSectionChanged{BoundModule, BootstrapConsumer.WorldEpoch});
	if (bRendering)
	{
		FActorSpawnParameters Parameters;
		Parameters.ObjectFlags |= RF_Transient;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Parameters);
		if (!Actor)
		{
			BootstrapAdmission = EVoxelTerrainResourceAdmission::InvalidRequest;
			return false;
		}
		Candidate->Actor = Actor;
		Actor->SetReplicates(false);
		Actor->SetActorEnableCollision(false);
		auto* Component = NewObject<UVoxelTerrainPrimitiveComponent>(Actor);
		Component->InitializeTerrain(Candidate);
		Candidate->Component = Component;
		Candidate->ComponentPin.Reset(Component);
		Actor->SetRootComponent(Component);
		Component->RegisterComponent();
		Candidate->Extension = FSceneViewExtensions::NewExtension<FVoxelTerrainViewExtension>(Candidate.ToSharedRef(), World->Scene);
	}
	State = MoveTemp(Candidate);
	State->InputLease->MarkPublished();
	return GetNativeStateAdmission() == EVoxelTerrainResourceAdmission::Granted;
}
