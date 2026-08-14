#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "MsClankerBase.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMsHealthComponent;

/**
 * Shared base for every clanker.
 *
 * Deliberately does NOT use NavMesh or AIController. Clankers steer directly toward the
 * player with simple vector maths, which means they work in any level with no navigation
 * build, and it scales to the crowd sizes a Megabonk-like needs. Pathfinding around complex
 * geometry can come later if districts demand it.
 *
 * All steering runs on the server only; clients see replicated movement.
 */
UCLASS(Abstract)
class MEATSPACE_API AMsClankerBase : public APawn
{
	GENERATED_BODY()

public:
	AMsClankerBase();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Meatspace|Clanker")
	UMsHealthComponent* GetHealth() const { return HealthComponent; }

protected:
	virtual void BeginPlay() override;

	/**
	 * Where this clanker wants to go this frame, as a unit vector. Subclasses define their
	 * own movement personality here - that is the only thing separating a ground swarm from
	 * an erratic flyer.
	 */
	virtual FVector ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer);

	/** Nearest player pawn, or null. */
	APawn* FindTargetPlayer() const;

	UFUNCTION()
	virtual void HandleDeath(AActor* DeadActor);

	/** Collision root. This is what the sword and gun actually hit. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Clanker")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Clanker")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Clanker")
	TObjectPtr<UMsHealthComponent> HealthComponent;

	/** Cruise speed in cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker", meta = (ClampMin = "0.0"))
	float MoveSpeed = 320.0f;

	/** How quickly it can change direction. Lower feels heavier and more predictable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker", meta = (ClampMin = "0.1"))
	float TurnResponsiveness = 6.0f;

	/** Stops closing in once this near the player. Prevents shoving the player around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker", meta = (ClampMin = "0.0"))
	float StopDistance = 110.0f;

	/** Ignores players further away than this. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker", meta = (ClampMin = "0.0"))
	float AggroRange = 0.0f;

	/** Turn to face the direction of travel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker")
	bool bFaceMovementDirection = true;

	/**
	 * Ground-walkers ride the surface: movement is horizontal only, and height is set by a
	 * downward trace each frame. Without this they sit flush in the floor, every swept move
	 * grazes it, and they end up rotating on the spot instead of advancing.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker|Ground")
	bool bSnapToGround = false;

	/** How far above the traced floor the actor's origin sits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker|Ground", meta = (ClampMin = "0.0"))
	float GroundOffset = 45.0f;

	/** How far down to look for a floor before giving up. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Meatspace|Clanker|Ground", meta = (ClampMin = "1.0"))
	float GroundTraceDistance = 400.0f;

	/** Snaps the actor onto the floor beneath it. */
	void SnapToGround();

	/** Smoothed current velocity. Used for steering and for flock alignment. */
	FVector CurrentVelocity = FVector::ZeroVector;

	/** Cached horizontal distance to the target this frame. */
	float DistanceToTarget = 0.0f;
};
