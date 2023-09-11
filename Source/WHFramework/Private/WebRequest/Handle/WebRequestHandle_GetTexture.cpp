// Fill out your copyright notice in the Description page of Project Settings.


#include "WebRequest/Handle/WebRequestHandle_GetTexture.h"

#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Interfaces/IHttpResponse.h"

UWebRequestHandle_GetTexture::UWebRequestHandle_GetTexture()
{
	Texture = nullptr;
}

void UWebRequestHandle_GetTexture::OnDespawn_Implementation(bool bRecovery)
{
	Super::OnDespawn_Implementation(bRecovery);

	Texture = nullptr;
}

void UWebRequestHandle_GetTexture::Fill(const FWebRequestResult& InResult, const TArray<FParameter>& InParams)
{
    if ( InResult.bSucceeded && InResult.HttpResponse->GetContentLength() > 0 )
    {
        IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> ImageWrappers[4] =
        {
            ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG),
            ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG),
            ImageWrapperModule.CreateImageWrapper(EImageFormat::BMP),
            ImageWrapperModule.CreateImageWrapper(EImageFormat::EXR)
        };

        for ( auto ImageWrapper : ImageWrappers )
        {
            if ( ImageWrapper.IsValid() && ImageWrapper->SetCompressed(InResult.HttpResponse->GetContent().GetData(), InResult.HttpResponse->GetContentLength()) )
            {
                TArray<uint8> RawData;
                constexpr ERGBFormat RGBFormat = ERGBFormat::BGRA;
                constexpr EPixelFormat PixelFormat = EPixelFormat::PF_FloatRGBA;
	            const FPixelFormatInfo& PixelFormatInfo = GPixelFormats[PixelFormat];
                if ( ImageWrapper->GetRaw(RGBFormat, 8, RawData) )
                {
                    const int32 SizeX = ImageWrapper->GetWidth();
                    const int32 SizeY = ImageWrapper->GetHeight();
                    if (SizeX > 0 && SizeY > 0 &&
                        (SizeX % PixelFormatInfo.BlockSizeX) == 0 &&
                        (SizeY % PixelFormatInfo.BlockSizeY) == 0)
                    {
                        Texture = NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient);
                        Texture->SRGB = true;

                        const auto PlatformData = new FTexturePlatformData();
                        Texture->SetPlatformData(PlatformData);
                        PlatformData->SizeX = SizeX;
                        PlatformData->SizeY = SizeY;
                        PlatformData->PixelFormat = PixelFormat;

                        // Allocate first mipmap.
                        const int32 NumBlocksX = SizeX / PixelFormatInfo.BlockSizeX;
                        const int32 NumBlocksY = SizeY / PixelFormatInfo.BlockSizeY;
                        const auto Mip = new FTexture2DMipMap();
                        PlatformData->Mips.Add(Mip);
                        Mip->SizeX = SizeX;
                        Mip->SizeY = SizeY;
                        Mip->BulkData.Lock(LOCK_READ_WRITE);
                        void* TextureData = Mip->BulkData.Realloc(NumBlocksX * NumBlocksY * PixelFormatInfo.BlockBytes);
                        FMemory::Memcpy(TextureData, RawData.GetData(), RawData.Num());
                        Mip->BulkData.Unlock();
                        
                        Texture->UpdateResource();
                    }
                }
                break;
            }
        }
    }
}
