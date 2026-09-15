
#pragma once

#include "Common/CommonModuleTypes.h"

#include "SaveGameModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSaveData()
	{
		bSaved = false;
		Datas = TArray<uint8>();
	}

	virtual ~FSaveData() { }

protected:
	UPROPERTY(Transient)
	bool bSaved;

	UPROPERTY(BlueprintReadWrite)
	TArray<uint8> Datas;

public:
	virtual bool IsValid() const { return true; }
	
	virtual bool IsSaved() const { return bSaved; }

	virtual void MakeSaved() { bSaved = true; }

	virtual TArray<uint8>& GetDatas() { return Datas; }

	virtual const TArray<uint8>& GetDatas() const { return Datas; }

	virtual void SetDatas(const TArray<uint8>& InDatas) { Datas = InDatas; }

public:
	template<class T>
	T* Cast()
	{
		return static_cast<T*>(this);
	}

	template<class T>
	T& CastRef()
	{
		return *Cast<T>();
	}
};

UENUM(BlueprintType)
enum class ESaveScope : uint8
{
	None,
	Profile,
	World
};

UENUM(BlueprintType)
enum class ESaveResultCode : uint8
{
	None,
	Success,
	InvalidArgument,
	NotFound,
	AlreadyExists,
	Busy,
	CaptureFailed,
	SerializeFailed,
	ReadFailed,
	WriteFailed,
	CommitFailed,
	CorruptData,
	VersionMismatch,
	ModuleMissing,
	LoadFailed,
	Unsupported,
	Unknown
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveOperationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly)
	ESaveResultCode Code = ESaveResultCode::Unknown;

	UPROPERTY(BlueprintReadOnly)
	FText Message;

	static FSaveOperationResult Success();

	static FSaveOperationResult Failed(ESaveResultCode InCode, const FText& InMessage);

	FORCEINLINE operator bool() const
	{
		return bSuccess;
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveSlotHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid SaveId;

	bool IsValid() const
	{
		return SaveId.IsValid();
	}
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCreateSaveSlotParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName InitialMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCaptureCurrentWorld = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSaveSlotSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid SaveId;

	UPROPERTY(BlueprintReadOnly)
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly)
	FString Description;

	UPROPERTY(BlueprintReadOnly)
	FDateTime CreatedAt;

	UPROPERTY(BlueprintReadOnly)
	FDateTime UpdatedAt;

	UPROPERTY(BlueprintReadOnly)
	FName CurrentMap;

	UPROPERTY(BlueprintReadOnly)
	double PlayTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly)
	FString PreviewPath;

	UPROPERTY(BlueprintReadOnly)
	bool bValid = false;
};

USTRUCT()
struct WHFRAMEWORK_API FSaveManifest
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SaveId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	FDateTime CreatedAt;

	UPROPERTY()
	FDateTime UpdatedAt;

	UPROPERTY()
	FName CurrentMap;

	UPROPERTY()
	double PlayTimeSeconds = 0.0;

	UPROPERTY()
	FString PreviewFile = TEXT("preview.png");

	UPROPERTY()
	int32 StorageVersion = 1;

	UPROPERTY()
	int32 GameVersion = 1;

	UPROPERTY()
	int32 CurrentGeneration = 0;

	UPROPERTY()
	TMap<FName, int32> ModuleVersions;

	FSaveSlotSummary ToSummary(const FString& WorldDir) const;
};

struct FModuleSaveFileHeader
{
	static constexpr uint32 MagicValue = 0x44574D53;

	uint32 Magic = MagicValue;
	int32 StorageVersion = 1;
	FName ModuleName;
	int32 ModuleVersion = 1;
	int64 PayloadSize = 0;
	uint32 PayloadCrc = 0;

	friend FArchive& operator<<(FArchive& Ar, FModuleSaveFileHeader& Value)
	{
		Ar << Value.Magic;
		Ar << Value.StorageVersion;
		Ar << Value.ModuleName;
		Ar << Value.ModuleVersion;
		Ar << Value.PayloadSize;
		Ar << Value.PayloadCrc;
		return Ar;
	}
};

USTRUCT()
struct WHFRAMEWORK_API FPendingSaveLoadContext
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid SaveId;

	UPROPERTY()
	int32 Generation = 0;

	UPROPERTY()
	FName TargetMap;

	UPROPERTY()
	EPhase LoadPhase = EPhase::All;

	bool IsValid() const
	{
		return SaveId.IsValid() && Generation > 0 && !TargetMap.IsNone();
	}
};
