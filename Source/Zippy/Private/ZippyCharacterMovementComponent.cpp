// Fill out your copyright notice in the Description page of Project Settings.


#include "ZippyCharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "ZippyCharacter.h"

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
}

// why...?  It's like SetMoveFor... but instead of setting on FSavedMove from UZippyChar, it's setting UZippyChar from FSavedmove
// I think we're basically restoring our CharacterMovementComponent's variables, so that Phys* can do what it needs to
// while substepping.
void UZippyCharacterMovementComponent::FSavedMove_Zippy::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	UZippyCharacterMovementComponent* CharacterMovement = Cast<UZippyCharacterMovementComponent>(C->GetCharacterMovement());

	CharacterMovement->Safe_bWantsToSprint = Saved_bWantsToSprint;
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
}

bool UZippyCharacterMovementComponent::IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == InCustomMovementMode;
}