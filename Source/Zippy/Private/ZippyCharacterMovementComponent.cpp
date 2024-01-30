// Fill out your copyright notice in the Description page of Project Settings.


#include "ZippyCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "ZippyCharacter.h"
#include "Components/CapsuleComponent.h"

#pragma region FSavedMove_Zippy

// checks if we can combine these moves - current move and NewMove.
// if all move in a saved data is same as last - then just return true and cut down on bandwidth.
bool UZippyCharacterMovementComponent::FSavedMove_Zippy::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	FSavedMove_Zippy* NewZippyMove = static_cast<FSavedMove_Zippy*>(NewMove.Get());

	if (Saved_bWantsToSprint != NewZippyMove->Saved_bWantsToSprint)
	{
		return false;
	}

	// let super handle the rest - other things to check - accel, etc.
	return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

// reset a saved move object to be emptied.
void UZippyCharacterMovementComponent::FSavedMove_Zippy::Clear()
{
	FSavedMove_Character::Clear();
	
	Saved_bWantsToSprint = 0;
}

// how client replicates movement data.
uint8 UZippyCharacterMovementComponent::FSavedMove_Zippy::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (Saved_bWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

// look thru all the variables you want in movement component - set their respected "saved" variables.
void UZippyCharacterMovementComponent::FSavedMove_Zippy::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	UZippyCharacterMovementComponent* CharacterMovement = Cast<UZippyCharacterMovementComponent>(C->GetCharacterMovement());

	// grab what you want from CharacterMovement...
	Saved_bWantsToSprint = CharacterMovement->Safe_bWantsToSprint;
	Saved_bPrevWantsToCrouch = CharacterMovement->Safe_bPrevWantsToCrouch;
}

// why...?  It's like SetMoveFor... but instead of setting on FSavedMove from UZippyChar, it's setting UZippyChar from FSavedmove
// I think we're basically restoring our CharacterMovementComponent's variables, so that Phys* can do what it needs to
// while substepping.
void UZippyCharacterMovementComponent::FSavedMove_Zippy::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	UZippyCharacterMovementComponent* CharacterMovement = Cast<UZippyCharacterMovementComponent>(C->GetCharacterMovement());

	CharacterMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
	CharacterMovement->Safe_bPrevWantsToCrouch = Saved_bPrevWantsToCrouch;
}

#pragma endregion


#pragma region FNetworkPredictionData_Client_Zippy

UZippyCharacterMovementComponent::FNetworkPredictionData_Client_Zippy::FNetworkPredictionData_Client_Zippy(
	const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement)
{

}

FSavedMovePtr UZippyCharacterMovementComponent::FNetworkPredictionData_Client_Zippy::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Zippy());
}

#pragma endregion

UZippyCharacterMovementComponent::UZippyCharacterMovementComponent()
{
	Sprint_MaxWalkSpeed = 500.f;
	Walk_MaxWalkSpeed = 650.f;

	NavAgentProps.bCanCrouch = true;
}

FNetworkPredictionData_Client* UZippyCharacterMovementComponent::GetPredictionData_Client() const
{
	check(PawnOwner != nullptr)

	if (ClientPredictionData == nullptr)
	{
		UZippyCharacterMovementComponent* MutableThis = const_cast<UZippyCharacterMovementComponent*>(this);

		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Zippy(*this);
		MutableThis->ClientPredictionData->MaxSmoothNetUpdateDist = 92.f;
		MutableThis->ClientPredictionData->NoSmoothNetUpdateDist = 140.f;
	}
	return ClientPredictionData;
}

void UZippyCharacterMovementComponent::EnterSlide()
{
	bWantsToCrouch = true;
	Velocity += Velocity.GetSafeNormal2D() * Slide_EnterImpulse;
	SetMovementMode(MOVE_Custom, ECustomMovementMode::CMOVE_Slide);
}

void UZippyCharacterMovementComponent::ExitSlide()
{
	bWantsToCrouch = false;

	FQuat NewRotation = FRotationMatrix::MakeFromXZ(UpdatedComponent->GetForwardVector().GetSafeNormal2D(), FVector::UpVector).ToQuat();
	FHitResult Hit;
	// can only call in safe movement context...
	SafeMoveUpdatedComponent(FVector::ZeroVector, NewRotation, true, Hit);
	SetMovementMode(MOVE_Walking);
}

// reference: https://youtu.be/-iaw-ifiUok?si=lgLVbJ-OGnU-Psdt&t=649
void UZippyCharacterMovementComponent::PhysSlide(float DeltaTime, int32 Iterations)
{
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	FHitResult SurfaceHit;
	if (!GetSlideSurface(SurfaceHit) || Velocity.SizeSquared() < pow(Slide_MinSpeed, 2))
	{
		ExitSlide();
		StartNewPhysics(DeltaTime, Iterations);
		return;
	}

	// Surface Gravity holds you to the ground a bit.
	Velocity += Slide_GravityForce * FVector::DownVector * DeltaTime; // v += a * dt

	// Strafe
	// Only allow horizontal Acceleration (Acceleration is basically the input vector)
	float HorizontalAcceleration = FMath::Abs(FVector::DotProduct(Acceleration.GetSafeNormal(), UpdatedComponent->GetRightVector()));
	if (HorizontalAcceleration > 0.5)
	{
		Acceleration = Acceleration.ProjectOnTo(UpdatedComponent->GetRightVector());
	}
	else
	{
		Acceleration = FVector::ZeroVector;
	}

	// Calc Velocity;
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		// bFluid makes Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
		CalcVelocity(DeltaTime, Slide_Friction, true, GetMaxBrakingDeceleration());
	}
	ApplyRootMotionToVelocity(DeltaTime);

	// Perform Move
	Iterations++;
	bJustTeleported = false;

	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	FQuat OldRotation = UpdatedComponent->GetComponentRotation().Quaternion();
	FHitResult Hit(1.0f);
	FVector Adjusted = Velocity * DeltaTime; // dx = v * dt

	// Conform rotation to orientation of our sliding surface, SurfaceHit.
	FVector VelPlaneDir = FVector::VectorPlaneProject(Velocity, SurfaceHit.Normal).GetSafeNormal();
	FQuat NewRotation = FRotationMatrix::MakeFromXZ(VelPlaneDir, SurfaceHit.Normal).ToQuat();
	// Apply the Move!
	SafeMoveUpdatedComponent(Adjusted, NewRotation, true, Hit);

	if (Hit.Time < 1.f)
	{
		HandleImpact(Hit, DeltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	FHitResult NewSurfaceHit;
	if (!GetSlideSurface(NewSurfaceHit) || Velocity.SizeSquared() < pow(Slide_MinSpeed, 2))
	{
		ExitSlide();
	}

	if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / DeltaTime; // v = dx/dt
	}

}

bool UZippyCharacterMovementComponent::GetSlideSurface(FHitResult& Hit) const
{
	FVector Start = UpdatedComponent->GetComponentLocation();
	FVector End = Start + ZippyCharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.f * FVector::DownVector;
	FName ProfileName = TEXT("BlockAll");

	return GetWorld()->LineTraceSingleByProfile(Hit, Start, End, ProfileName, ZippyCharacterOwner->GetIgnoreCharacterParams());
}

void UZippyCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
	ZippyCharacterOwner = Cast<AZippyCharacter>(GetOwner());
}

void UZippyCharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	Safe_bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

#pragma region Input

void UZippyCharacterMovementComponent::SprintPressed()
{
	Safe_bWantsToSprint = true;
}

void UZippyCharacterMovementComponent::SprintReleased()
{
	Safe_bWantsToSprint = false;
}

void UZippyCharacterMovementComponent::CrouchPressed()
{
	bWantsToCrouch = !bWantsToCrouch;
}

#pragma endregion

void UZippyCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (MovementMode == MOVE_Walking)
	{
		if (Safe_bWantsToSprint)
		{
			MaxWalkSpeed = Sprint_MaxWalkSpeed;
		}
		else
		{
			MaxWalkSpeed = Walk_MaxWalkSpeed;
		}
	}

	Safe_bPrevWantsToCrouch = bWantsToCrouch;
}

bool UZippyCharacterMovementComponent::IsMovingOnGround() const
{
	return Super::IsMovingOnGround() || IsCustomMovementMode(CMOVE_Slide);
}

bool UZippyCharacterMovementComponent::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && IsMovingOnGround();
}

void UZippyCharacterMovementComponent::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (MovementMode == MOVE_Walking && !bWantsToCrouch && Safe_bPrevWantsToCrouch)
	{
		FHitResult PotentialSlideSurface;
		if (Velocity.SizeSquared() > pow(Slide_MinSpeed, 2) && GetSlideSurface(PotentialSlideSurface))
		{
			EnterSlide();
		}
	}

	if (IsCustomMovementMode(CMOVE_Slide) && !bWantsToCrouch)
	{
		ExitSlide();
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UZippyCharacterMovementComponent::PhysCustom(float DeltaTime, int32 Iterations)
{
	Super::PhysCustom(DeltaTime, Iterations);

	switch (CustomMovementMode)
	{
		case ECustomMovementMode::CMOVE_Slide:
			PhysSlide(DeltaTime, Iterations);
			break;
		default:
			UE_LOG(LogTemp, Fatal, TEXT("Invalid Movement Mode"))
	}
}

bool UZippyCharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}