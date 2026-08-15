#include "Character/MsAnimInstance.h"

#include "Character/MsCharacter.h"
#include "Combat/MsMeleeComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

void UMsAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	OwningCharacter = Cast<AMsCharacter>(TryGetPawnOwner());
	if (OwningCharacter)
	{
		Movement = OwningCharacter->GetCharacterMovement();
		PreviousYaw = OwningCharacter->GetActorRotation().Yaw;
	}
}

void UMsAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// The owner can arrive late - possession order is not guaranteed - so keep trying rather
	// than giving up after the first frame.
	if (!OwningCharacter)
	{
		OwningCharacter = Cast<AMsCharacter>(TryGetPawnOwner());
		if (!OwningCharacter)
		{
			return;
		}
		Movement = OwningCharacter->GetCharacterMovement();
		PreviousYaw = OwningCharacter->GetActorRotation().Yaw;
	}

	const FVector Velocity = OwningCharacter->GetVelocity();
	const FRotator ActorRotation = OwningCharacter->GetActorRotation();

	GroundSpeed = Velocity.Size2D();
	VerticalVelocity = Velocity.Z;

	// Direction of travel relative to facing. Computed by hand rather than via the animation
	// library so this module needs no extra dependency.
	FVector FlatVelocity = Velocity;
	FlatVelocity.Z = 0.0f;

	if (FlatVelocity.SizeSquared() > FMath::Square(MoveThreshold))
	{
		const FVector VelocityDirection = FlatVelocity.GetSafeNormal();
		const FVector Forward = ActorRotation.Vector();
		const FVector Right = FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::Y);

		const float ForwardDot = FVector::DotProduct(Forward, VelocityDirection);
		const float RightDot = FVector::DotProduct(Right, VelocityDirection);

		Direction = FMath::RadiansToDegrees(FMath::Atan2(RightDot, ForwardDot));
	}
	// Otherwise hold the last direction: recomputing it from near-zero velocity produces
	// garbage, and the blendspace would snap as the character comes to rest.

	if (Movement)
	{
		bIsInAir = Movement->IsFalling();

		// Acceleration rather than speed, so the idle plays the moment input stops instead of
		// waiting for momentum to bleed off.
		const bool bHasInput = Movement->GetCurrentAcceleration().SizeSquared2D() > 0.0f;
		bShouldMove = bHasInput && GroundSpeed > MoveThreshold;
	}

	// Turn rate, for leaning into turns.
	const float CurrentYaw = ActorRotation.Yaw;
	const float RawTurnRate = DeltaSeconds > 0.0f
		? FRotator::NormalizeAxis(CurrentYaw - PreviousYaw) / DeltaSeconds
		: 0.0f;
	PreviousYaw = CurrentYaw;

	TurnRate = FMath::FInterpTo(TurnRate, RawTurnRate, DeltaSeconds, TurnRateSmoothing);

	// --- Aim offset ---
	//
	// The character faces where the camera looks, so aim yaw is usually near zero. Pitch is
	// the one that matters: it lets the upper body point up at flying clankers instead of
	// firing level while the reticle is in the sky.
	FVector AimPoint;
	if (OwningCharacter->ComputeAimPoint(AimPoint))
	{
		const FVector ToAim = AimPoint - OwningCharacter->GetActorLocation();
		if (!ToAim.IsNearlyZero())
		{
			const FRotator AimRotation = ToAim.Rotation();
			const FRotator Delta = (AimRotation - ActorRotation).GetNormalized();

			// Clamped to what a body can plausibly twist to. Beyond this the character turns
			// instead, which they already do by facing the camera.
			AimPitch = FMath::ClampAngle(Delta.Pitch, -75.0f, 75.0f);
			AimYaw = FMath::ClampAngle(Delta.Yaw, -90.0f, 90.0f);
		}
	}

	bIsAiming = OwningCharacter->IsAiming();
	ActiveSlot = OwningCharacter->GetActiveSlot();
	bHasWeapon = OwningCharacter->HasAnyWeapon();
	bIsDead = OwningCharacter->IsDead();

	if (const UMsMeleeComponent* Melee = OwningCharacter->GetMelee())
	{
		bIsSwinging = Melee->IsSwinging();
	}
}
