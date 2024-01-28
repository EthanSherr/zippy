// Fill out your copyright notice in the Description page of Project Settings.


#include "ZippyCameraManager.h"
#include "ZippyCharacter.h"
#include "ZippyCharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

AZippyCameraManager::AZippyCameraManager()
{

}

void AZippyCameraManager::UpdateViewTarget(FTViewTarget& OutVT, float DeltaTime)
{
	Super::UpdateViewTarget(OutVT, DeltaTime);

	if (AZippyCharacter* ZippyCharacter = Cast<AZippyCharacter>(GetOwningPlayerController()->GetPawn()))
	{
		UZippyCharacterMovementComponent* ZMC = ZippyCharacter->GetZippyCharacterMovement();
		float CurrentCrouchHalfHeight = ZMC->GetCrouchedHalfHeight();
		float OriginalHalfHeight = ZippyCharacter->GetClass()->GetDefaultObject<ACharacter>()->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
		//UE_LOG(LogTemp, Warning, TEXT("CurrentCrouchHalfHeight %f, OriginalCrouchHalfHeight: %f"), CurrentCrouchHalfHeight, OriginalHalfHeight)
		FVector TargetCrouchOffset = FVector(
			0,
			0,
			CurrentCrouchHalfHeight - OriginalHalfHeight
		);
		FVector Offset = FMath::Lerp(FVector::ZeroVector, TargetCrouchOffset, FMath::Clamp(CrouchBlendTime / CrouchBlendDuration, 0.f, 1.0f));
		if (ZMC->IsCrouching())
		{
			CrouchBlendTime = FMath::Clamp(CrouchBlendTime + DeltaTime, 0.f, CrouchBlendDuration);
			//UE_LOG(LogTemp, Warning, TEXT("OFFSET(%s) - (TargetCrouchOffset) %s = %s"), *Offset.ToString(), *TargetCrouchOffset.ToString(), *(Offset - TargetCrouchOffset).ToString())
			//*See note 1) at bottom
			Offset -= TargetCrouchOffset; // https://youtu.be/vw4sPZ8xhFk?si=LIqdSoJ5itD3_AQ4&t=832
		}
		else
		{
			CrouchBlendTime = FMath::Clamp(CrouchBlendTime - DeltaTime, 0.f, CrouchBlendDuration);
		}

		// weird byproduct of crouch impl
		// camera doesn't change position if you crouch midjump
		if (ZMC->IsMovingOnGround())
		{
			OutVT.POV.Location += Offset;
		}
	}
}

// 1)
// Let's say our OriginalHalfHeight = 80
// and our OriginalHalfHeight = 40.
// Our TargetCrouchOffset.Z = 40 - 80 = -40;
// Therefore Offset starts at a range from [0, -40] as we crouch.
// We have to instead have a range from [40, 0] - because as soon as we crouch the camera is lower! (-40 what it used to be, altho it's transform.location is 0,0,0)
// so the line Offset -= TargetCrouchOffset will help us invert that range [0,-40] by - (-40) =  [0, -40] + 40 => [40, 0]