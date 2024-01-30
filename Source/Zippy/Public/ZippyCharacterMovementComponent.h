// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ZippyCharacterMovementComponent.generated.h"

class AZippyCharacter;

UENUM(BlueprintType)
enum ECustomMovementMode
{
	CMOVE_None UMETA(Hidden),
	CMOVE_Slide UMETA(DisplayName = "Slide"),
	CMOVE_Max UMETA(Hidden),
};



UCLASS()
class ZIPPY_API UZippyCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	// SavedMove state!  Acceleration
	class FSavedMove_Zippy : public FSavedMove_Character
	{
		typedef FSavedMove_Character Super;
		// Flag
		uint8 Saved_bWantsToSprint : 1;

		// not replicated?
		// if it was false, but is now true - we'd like to probably appy our slide...
		// saving this variable to recreate our crouch event.
		/*
			DELGOODIE:
			Re Saved_bPrevWantsToCrouch
			Great video!

			I had a hard time understanding how FSavedMove_Zippy.Saved_bPrevWantsToCrouch gets applied to movement on the server as well as client - because the Saved_bPrevWantsToCrouch never is replicated (as you say).
			But I get it now.  Saved_bWantsToSprint is derived from bWantsToSprint which is replicated.  So our Saved_bPrevWantsToCrouch is derived from fliping bWantsToSprint.

			I feel this is a little weird though.  With this approach we don't have good control to increase the "double tap crouch" threshold - and I feel the implementation is coupled to the input.
			I think I'd rather have my own flag for this behavior then I'd have finer control.  I could also decide in the air if when I land I'd like to go into a slide - because I'd not be relying on any previous bwantsToCrouch.

		Let me know if I'm missing something.
		*/
		uint8 Saved_bPrevWantsToCrouch:1;

		// used for optimization, if same as last save move - skip!
		virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
		virtual void Clear() override;
		// SavedMove's compressed flags serialized here...
		virtual uint8 GetCompressedFlags() const override;
		// saves state from character movement
		virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
		// saves state to character movement
		virtual void PrepMoveFor(ACharacter* C) override;
	};

	// indicate that we want to use FSavedMove_Zippy
	class FNetworkPredictionData_Client_Zippy : public FNetworkPredictionData_Client_Character
	{
	public:
		FNetworkPredictionData_Client_Zippy(const UCharacterMovementComponent& CilentMovement);
		typedef FNetworkPredictionData_Client_Character Super;
		virtual FSavedMovePtr AllocateNewMove() override;
	};

	// set, and forget.  
	bool Safe_bWantsToSprint;
	bool Safe_bPrevWantsToCrouch;

public:
	UZippyCharacterMovementComponent();
public:
	// using our FNetworkPredictionDatay_Client_Zippy type
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

private:
	void EnterSlide();
	void ExitSlide();
	void PhysSlide(float DeltaTime, int32 Iterations);
	bool GetSlideSurface(FHitResult& Hit) const;

protected:

	virtual void InitializeComponent() override;

	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	// called at any perform move. 
	// write movemenet logic regardless of our movement mode
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

public:
	virtual bool IsMovingOnGround() const override;
	virtual bool CanCrouchInCurrentState() const override;

protected:
	// crouch is handled here, do this before crouch triggers
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

	virtual void PhysCustom(float DeltaTime, int32 Iterations) override;

public:
	UFUNCTION(BlueprintCallable) void SprintPressed();
	UFUNCTION(BlueprintCallable) void SprintReleased();

	UFUNCTION(BlueprintCallable) void CrouchPressed();

	// Parameters
	UPROPERTY(EditDefaultsOnly) float Sprint_MaxWalkSpeed;
	UPROPERTY(EditDefaultsOnly) float Walk_MaxWalkSpeed;

	UPROPERTY(EditDefaultsOnly) float Slide_MinSpeed = 350;
	UPROPERTY(EditDefaultsOnly) float Slide_EnterImpulse = 500;
	UPROPERTY(EditDefaultsOnly) float Slide_GravityForce = 5000;
	UPROPERTY(EditDefaultsOnly) float Slide_Friction = 1.3;


	UPROPERTY(Transient) AZippyCharacter* ZippyCharacterOwner;

	UFUNCTION(BlueprintPure) bool IsCustomMovementMode(ECustomMovementMode InCustomMovementMode) const;
};
