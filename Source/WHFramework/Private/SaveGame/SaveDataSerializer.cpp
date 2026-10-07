#include "SaveGame/SaveDataSerializer.h"

#include "SaveGame/SaveGameModuleTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "WHFrameworkCoreTypes.h"

namespace
{
	class FBoundedParameterWriter final : public FArchive
	{
	public:
		using FArchive::operator<<;

		FBoundedParameterWriter(TArray<uint8>& InBytes, const int32 InMaximumBytes,
			TFunctionRef<bool(TArray<uint8>&, int32)> InReserve)
			: Bytes(InBytes), MaximumBytes(InMaximumBytes), Reserve(InReserve)
		{
			SetIsSaving(true);
			SetIsPersistent(true);
		}

		virtual FArchive& operator<<(FName& InName) override
		{
			// 与原 FMemoryWriter 的名称格式一致，不能使用 FArchive 的默认空实现。
			FString Name = InName.ToString();
			*this << Name;
			return *this;
		}

		virtual void Serialize(void* InData, const int64 InLength) override
		{
			if (IsError()) return;
			if (InLength < 0 || Position < 0 || Position > MaximumBytes || InLength > MaximumBytes - Position)
			{
				SetError();
				return;
			}
			const int32 Required = int32(Position + InLength);
			if (Required > Bytes.Max() && (!Reserve(Bytes, Required) || Required > Bytes.Max()))
			{
				SetError();
				return;
			}
			if (Required > Bytes.Num()) Bytes.SetNumZeroed(Required, EAllowShrinking::No);
			if (InLength > 0) FMemory::Memcpy(Bytes.GetData() + Position, InData, InLength);
			Position = Required;
		}

		virtual void Seek(const int64 InPosition) override
		{
			if (InPosition < 0 || InPosition > MaximumBytes) SetError();
			else Position = InPosition;
		}
		virtual int64 Tell() override
		{
			return Position;
		}
		virtual int64 TotalSize() override
		{
			return Bytes.Num();
		}
		virtual FString GetArchiveName() const override
		{
			return TEXT("BoundedSaveParameterWriter");
		}

	private:
		TArray<uint8>& Bytes;
		int32 MaximumBytes;
		TFunctionRef<bool(TArray<uint8>&, int32)> Reserve;
		int64 Position = 0;
	};
}

bool FSaveDataSerializer::SerializeParameterBounded(const FParameter& InData, const int32 InMaximumBytes,
	TArray<uint8>& OutBytes, TFunctionRef<bool(TArray<uint8>&, int32)> InReserve)
{
	if (!InData.HasValue() || InMaximumBytes < 0) return false;
	OutBytes.Reset();
	FBoundedParameterWriter Writer(OutBytes, InMaximumBytes, InReserve);
	FSaveDataArchive Archive(Writer);
	Archive.ArIsSaveGame = false;
	FParameter::StaticStruct()->SerializeItem(Archive, const_cast<FParameter*>(&InData), nullptr);
	return !Writer.IsError() && !Archive.IsError();
}

bool FSaveDataSerializer::SerializeParameter(const FParameter& InData, TArray<uint8>& OutBytes)
{
	if(!InData.HasValue())
	{
		return false;
	}

	OutBytes.Reset();
	FMemoryWriter Writer(OutBytes, true);
	FSaveDataArchive Archive(Writer);
	Archive.ArIsSaveGame = false;
	FParameter::StaticStruct()->SerializeItem(Archive, const_cast<FParameter*>(&InData), nullptr);
	return !Archive.IsError();
}

bool FSaveDataSerializer::DeserializeParameter(const TArray<uint8>& InBytes, FParameter& OutData)
{
	if(InBytes.IsEmpty())
	{
		return false;
	}

	FMemoryReader Reader(InBytes, true);
	FSaveDataArchive Archive(Reader);
	Archive.ArIsSaveGame = false;
	OutData.Reset();
	FParameter::StaticStruct()->SerializeItem(Archive, &OutData, nullptr);
	if(Archive.IsError() || !OutData.HasValue())
	{
		OutData.Reset();
		return false;
	}

	MarkSaveDataSaved(OutData);
	return true;
}

bool FSaveDataSerializer::DeserializeParameterView(const TConstArrayView<uint8> InBytes, FParameter& OutData)
{
	check(IsInGameThread());
	if (InBytes.IsEmpty()) return false;
	FMemoryReaderView Reader(FMemoryView(InBytes.GetData(), uint64(InBytes.Num())), true);
	Reader.ArMaxSerializeSize = InBytes.Num();
	FSaveDataArchive Archive(Reader);
	Archive.ArIsSaveGame = false;
	OutData.Reset();
	FParameter::StaticStruct()->SerializeItem(Archive, &OutData, nullptr);
	if (Reader.IsError() || Archive.IsError() || Reader.Tell() != Reader.TotalSize() ||
		!OutData.HasValue() || !MarkSaveDataSaved(OutData))
	{
		OutData.Reset();
		return false;
	}
	return true;
}

bool FSaveDataSerializer::MarkSaveDataSaved(FParameter& InOutData)
{
	const UScriptStruct* Struct = InOutData.GetStructType();
	uint8* Memory = InOutData.GetMutableStructMemory();
	if(!Struct || !Memory || !Struct->IsChildOf(FSaveData::StaticStruct()))
	{
		return false;
	}

	reinterpret_cast<FSaveData*>(Memory)->MakeSaved();
	return true;
}

bool FSaveDataSerializer::BuildModuleFile(FName ModuleName, int32 ModuleVersion, const FParameter& Data, TArray<uint8>& OutFileBytes)
{
	TArray<uint8> Payload;
	if(!SerializeParameter(Data, Payload))
	{
		return false;
	}

	FModuleSaveFileHeader Header;
	Header.ModuleName = ModuleName;
	Header.ModuleVersion = ModuleVersion;
	Header.PayloadSize = Payload.Num();
	Header.PayloadCrc = Payload.IsEmpty() ? 0 : FCrc::MemCrc32(Payload.GetData(), Payload.Num());

	OutFileBytes.Reset();
	FMemoryWriter Writer(OutFileBytes, true);
	Writer << Header;
	Writer.Serialize(Payload.GetData(), Payload.Num());
	return !Writer.IsError();
}

bool FSaveDataSerializer::WriteModuleHeaderBounded(const FName InModuleName, const int32 InModuleVersion,
	const int64 InPayloadSize, const uint32 InPayloadCrc, TArray<uint8>& OutFileBytes,
	TFunctionRef<bool(TArray<uint8>&, int32)> InReserve)
{
	if (InPayloadSize <= 0 || InPayloadSize > MAX_int32) return false;
	FModuleSaveFileHeader Header;
	Header.ModuleName = InModuleName;
	Header.ModuleVersion = InModuleVersion;
	Header.PayloadSize = InPayloadSize;
	Header.PayloadCrc = InPayloadCrc;
	OutFileBytes.Reset();
	FBoundedParameterWriter Writer(OutFileBytes, MAX_int32, InReserve);
	Writer << Header;
	return !Writer.IsError();
}

bool FSaveDataSerializer::ReadModuleFile(const TArray<uint8>& InFileBytes, FModuleSaveFileHeader& OutHeader, FParameter& OutData)
{
	if(InFileBytes.IsEmpty())
	{
		return false;
	}

	FMemoryReader Reader(InFileBytes, true);
	Reader << OutHeader;
	if(Reader.IsError() ||
		OutHeader.Magic != FModuleSaveFileHeader::MagicValue ||
		OutHeader.StorageVersion != FModuleSaveFileHeader::CurrentStorageVersion)
	{
		return false;
	}

	const int64 Remaining = Reader.TotalSize() - Reader.Tell();
	if(OutHeader.PayloadSize <= 0 || OutHeader.PayloadSize > Remaining || OutHeader.PayloadSize > MAX_int32)
	{
		return false;
	}

	TArray<uint8> Payload;
	Payload.SetNumUninitialized(static_cast<int32>(OutHeader.PayloadSize));
	Reader.Serialize(Payload.GetData(), Payload.Num());
	if(Reader.IsError() || FCrc::MemCrc32(Payload.GetData(), Payload.Num()) != OutHeader.PayloadCrc)
	{
		return false;
	}

	return DeserializeParameter(Payload, OutData);
}
