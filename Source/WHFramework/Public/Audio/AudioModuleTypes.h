// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Parameter/ParameterValueTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Main/NetworkDispatchTypes.h"

#include "AudioModuleTypes.generated.h"

class USoundBase;

DECLARE_DYNAMIC_DELEGATE(FOnSoundPlayFinishDelegate);

UENUM(BlueprintType)
enum class EAudioNetworkSoundState : uint8
{
	Unknown,
	Pending,
	Authorized,
	// 服务器已发起广播；各客户端的实际播放可能尚未开始。
	Started,
	Rejected,
	Finished,
	Cancelled,
	TimedOut
};

USTRUCT(BlueprintType)
struct FSingleSoundHandle
{
	GENERATED_USTRUCT_BODY()

	FSingleSoundHandle()
		: Handle(INDEX_NONE)
	{
	}

	FSingleSoundHandle(int32 InHandle, bool bInNetwork = false)
		: Handle(InHandle), bNetwork(bInNetwork)
	{
	}

	static FSingleSoundHandle Pending(const FGuid& InRequestId)
	{
		FSingleSoundHandle Result;
		Result.RequestId = InRequestId;
		return Result;
	}

	bool IsValid() const
	{
		return Handle != INDEX_NONE || RequestId.IsValid();
	}
	bool IsPending() const { return RequestId.IsValid(); }
	const FGuid& GetRequestId() const { return RequestId; }

	bool operator==(const FSingleSoundHandle& Other) const
	{
		return Handle == Other.Handle && bNetwork == Other.bNetwork && RequestId == Other.RequestId;
	}

	bool operator!=(const FSingleSoundHandle& Other) const
	{
		return !(*this == Other);
	}

	friend uint32 GetTypeHash(const FSingleSoundHandle& SpecHandle)
	{
		return HashCombine(HashCombine(::GetTypeHash(SpecHandle.Handle), ::GetTypeHash(static_cast<uint8>(SpecHandle.bNetwork))), GetTypeHash(SpecHandle.RequestId));
	}

	FString ToString() const
	{
		return IsPending() ? RequestId.ToString() : Handle != INDEX_NONE ? FString::FromInt(Handle) : TEXT("Invalid");
	}

private:
	UPROPERTY()
	int32 Handle;

	UPROPERTY()
	bool bNetwork = false;

	UPROPERTY()
	FGuid RequestId;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSoundPlaybackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FSingleSoundHandle Handle;

	UPROPERTY(BlueprintReadOnly)
	FNetworkDispatchResult Dispatch;
};

USTRUCT(BlueprintType)
struct FSingleSoundInfo
{
	GENERATED_USTRUCT_BODY()

	FSingleSoundInfo()
	{
		Sound = nullptr;
		Audio = nullptr;
	}

	FSingleSoundInfo(UAudioComponent* InAudio, USoundBase* InSound)
	{
		Sound = InSound;
		Audio = InAudio;
	}

	bool IsValid() const
	{
		return Audio != nullptr;
	}

public:
	UPROPERTY(BlueprintReadOnly)
	USoundBase* Sound;

	UPROPERTY(BlueprintReadOnly)
	UAudioComponent* Audio;
};

USTRUCT(BlueprintType)
struct FSoundParams : public FSaveData
{
	GENERATED_USTRUCT_BODY()

	FSoundParams()
	{
		Volume = 0.f;
		Pitch = 0.f;
	}

	FSoundParams(float InVolume, float InPitch)
	{
		Volume = InVolume;
		Pitch = InPitch;
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.0", ClampMax = "1.0", SettingApply = "Preview", SettingScale = "100.0", SettingDecimalPlaces = "0", Units = "%"), Category = "Volume")
	float Volume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (ClampMin = "0.5", ClampMax = "2.0", SettingApply = "Preview"), Category = "Pitch")
	float Pitch;
};

USTRUCT()
struct FSoundParamsParameterValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY()
	FSoundParams Value;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FAudioModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FAudioModuleSaveData()
	{
		GlobalSoundParams = FSoundParams(1.f, 1.f);
		BackgroundSoundParams = FSoundParams(1.f, 1.f);
		EnvironmentSoundParams = FSoundParams(1.f, 1.f);
		EffectSoundParams = FSoundParams(1.f, 1.f);
	}

public:
	UPROPERTY()
	FSoundParams GlobalSoundParams;

	UPROPERTY()
	FSoundParams BackgroundSoundParams;

	UPROPERTY()
	FSoundParams EnvironmentSoundParams;

	UPROPERTY()
	FSoundParams EffectSoundParams;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();

		GlobalSoundParams.MakeSaved();
		BackgroundSoundParams.MakeSaved();
		EnvironmentSoundParams.MakeSaved();
		EffectSoundParams.MakeSaved();
	}
};
