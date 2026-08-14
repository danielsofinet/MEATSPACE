#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MsMeleeComponent.generated.h"

class UAnimMontage;

/** Where a swing currently is in its lifecycle. */
UENUM(BlueprintType)
enum class EMsSwingPhase : uint8
{
	/** Not swinging. */
	Idle,

	/** Wind-up. Committed, but the blade is not dangerous yet. */
	Windup,

	/** The blade is live and sweeping. This is the only phase that deals damage. */
	Active,

	/** Follow-through. Cannot swing again until this ends. */
	Recovery
};

/**
 * One attack in the combo chain.
 *
 * Default chain: right -> left -> full circle. Click again during a swing to queue the next
 * step; stop clicking and the chain resets.
 */
USTRUCT(BlueprintType)
struct FMsSwingStep
{
	GENERATED_BODY()

	/** Total sweep angle. 360 is a full spin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (ClampMin = "10.0", ClampMax = "360.0"))
	float ArcDegrees = 160.0f;

	/** False sweeps left-to-right, true sweeps right-to-left. Alternate these so the chain reads as a rhythm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
	bool bRightToLeft = false;

	/** Scales the component's base Damage for this step. Finishers should hit harder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	/** Scales the component's base Reach for this step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing", meta = (ClampMin = "0.1"))
	float ReachMultiplier = 1.0f;

	/** Commit delay before the blade goes live. Higher = heavier, more readable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float WindupTime = 0.10f;

	/** How long the blade is dangerous. This is the swing itself. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.01"))
	float ActiveTime = 0.16f;

	/** Follow-through before the next swing can begin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
	float RecoveryTime = 0.22f;

	/** Optional per-step animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
	TObjectPtr<UAnimMontage> Montage = nullptr;
};

/**
 * Sword. Swept-sector melee, server-authoritative.
 *
 * Hit volume is deliberately generous rather than realistic: a vertical cylinder around the
 * character, from the ground to above the head, out to Reach. A target is hit when the swing's
 * angular sweep passes over it - at any distance from the character's feet outward, at any
 * height. That means clipping a clanker with the base of the blade counts, and so does hitting
 * something crouched or hovering just overhead.
 *
 * Each Active frame only tests the angular slice covered since the last frame, so a fast swing
 * still reads as a swing travelling through space rather than an instant cone of death. Each
 * actor can only be hit once per swing, which is what makes cleaving a crowd fair.
 *
 * Same netcode shape as the gun: the swinging client runs the whole state machine locally for
 * feel, but only the server's copy applies damage.
 */
UCLASS(ClassGroup = (Meatspace), meta = (BlueprintSpawnableComponent))
class MEATSPACE_API UMsMeleeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMsMeleeComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** Swing. Called again mid-swing, this queues the next step of the combo. */
	UFUNCTION(BlueprintCallable, Category = "Meatspace|Melee")
	void StartSwing();

	UFUNCTION(BlueprintPure, Category = "Meatspace|Melee")
	bool IsSwinging() const { return Phase != EMsSwingPhase::Idle; }

	UFUNCTION(BlueprintPure, Category = "Meatspace|Melee")
	EMsSwingPhase GetPhase() const { return Phase; }

	/** Which step of the combo is currently executing. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Melee")
	int32 GetComboIndex() const { return ComboIndex; }

	// --- Tunables ---

	/** Base damage per target hit, before the step's multiplier. Applies once per actor per swing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee")
	float Damage = 45.0f;

	/** Base reach in cm, measured from the character outward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee", meta = (ClampMin = "10.0"))
	float Reach = 260.0f;

	/**
	 * Half-height of the hit cylinder, in cm. 110 covers ground to just above the head of a
	 * standard character. Raise it to catch hovering clankers with a ground swing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee", meta = (ClampMin = "10.0"))
	float HitHalfHeight = 110.0f;

	/**
	 * Extra degrees added to each side of the swept slice. Pure generosity - it widens the
	 * window in which a target counts as hit. Raise it if swings feel like they should have
	 * connected.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee", meta = (ClampMin = "0.0", ClampMax = "90.0"))
	float AngleTolerance = 12.0f;

	/**
	 * If true, a target's physical size widens its hit window - big clankers are easier to
	 * clip than their centre point alone would allow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee")
	bool bAccountForTargetSize = true;

	/** The combo chain. Default: right, then left, then a full spin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Combo")
	TArray<FMsSwingStep> ComboSteps;

	/**
	 * How long after a swing ends the chain stays open. Click again inside this window to
	 * continue the combo; let it lapse and the next click starts from step 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Combo", meta = (ClampMin = "0.0"))
	float ComboResetTime = 0.7f;

	/** Draw the swept sector so you can see exactly what the blade covered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Melee|Debug")
	bool bDrawDebugSwing = true;

protected:
	/** Client -> server. Sends the step index so both sides run the same attack. */
	UFUNCTION(Server, Reliable)
	void ServerSwing(uint8 StepIndex);

	/** Starts a specific step of the chain. Runs on both the local client and the server. */
	void BeginSwing(int32 StepIndex);

	/** Applies damage to anything inside the angular slice covered since the last frame. */
	void SweepSlice(float PrevAlpha, float NewAlpha);

	/** Signed horizontal angle in degrees from the owner's forward. Positive = to the right. */
	float SignedAngleToTarget(const FVector& TargetLocation) const;

	/** Start and end angle of the current step, in degrees relative to forward. */
	void GetStepAngles(float& OutStartAngle, float& OutEndAngle) const;

	const FMsSwingStep& GetCurrentStep() const;

	void PlayStepMontage();
	void DrawSwingDebug(float PrevAngle, float NewAngle) const;

private:
	EMsSwingPhase Phase = EMsSwingPhase::Idle;

	/** Seconds spent in the current phase. */
	float PhaseTime = 0.0f;

	/** Progress through the arc during Active, 0..1. */
	float SwingAlpha = 0.0f;

	/** Which step of ComboSteps is running. */
	int32 ComboIndex = 0;

	/** Set when the player clicks mid-swing - consumed when the current swing finishes. */
	bool bComboQueued = false;

	/** When the last swing finished, for combo-window timing. */
	float LastSwingEndTime = -1000.0f;

	/** One hit per actor per swing. */
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
};
