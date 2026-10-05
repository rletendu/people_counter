#include "buzzer.h"

#include "config.h"

void playBlockedAlert() {
  tone(BUZZER_PIN, BLOCKED_BEEP_FIRST_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
  tone(BUZZER_PIN, BLOCKED_BEEP_SECOND_HZ, BEEP_DURATION_MS);
}

void playMenuEnterSound() {
  tone(BUZZER_PIN, MENU_ENTER_BEEP_FIRST_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
  tone(BUZZER_PIN, MENU_ENTER_BEEP_SECOND_HZ, BEEP_DURATION_MS);
}

void playMenuExitSound() {
  tone(BUZZER_PIN, MENU_EXIT_BEEP_HZ, BEEP_DURATION_MS);
}

void playResetConfirmSound() {
  tone(BUZZER_PIN, RESET_BEEP_FIRST_HZ, BEEP_DURATION_MS);
  delay(LOOP_DELAY_MS);
  tone(BUZZER_PIN, RESET_BEEP_SECOND_HZ, BEEP_DURATION_MS);
}

void playClickSound() {
  tone(BUZZER_PIN, CLICK_BEEP_HZ, CLICK_DURATION_MS);
}
