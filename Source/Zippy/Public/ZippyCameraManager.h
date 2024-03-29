#pragma once

#include "CoreMinimal.h"
#include "Camera/PlayerCameraManager.h"
#include "ZippyCameraManager.generated.h"


UCLASS()
class ZIPPY_API AZippyCameraManager : public APlayerCameraManager
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly) float CrouchBlendDuration = 0.5f;

	float CrouchBlendTime = 0.0f;

public:
	AZippyCameraManager();

	virtual void UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime) override;
};
