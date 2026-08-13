#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "MsCharacter.generated.h"

class UMsWeaponComponent;

/**
 * Base player character for MEATSPACE.
 *
 * Deliberately thin right now. It exists so combat has a C++ home to live in - the existing
 * BP_ThirdPersonCharacter gets reparented to this, which keeps the template's mesh, camera
 * and animation setup intact while moving ownership of gameplay into code.
 */
UCLASS()
class MEATSPACE_API AMsCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AMsCharacter();

	/** The gun. Anti-air weapon; the sword comes later. */
	UFUNCTION(BlueprintPure, Category = "Meatspace|Combat")
	UMsWeaponComponent* GetWeapon() const { return Weapon; }

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Meatspace|Combat")
	TObjectPtr<UMsWeaponComponent> Weapon;

private:
	void OnFirePressed();
	void OnFireReleased();
};
