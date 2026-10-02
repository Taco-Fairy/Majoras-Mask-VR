#ifndef SAVING_ENHANCEMENTS_H
#define SAVING_ENHANCEMENTS_H

#ifdef MMVR_ENABLE
#define SAVING_ENHANCEMENTS_DEFAULT_ENABLED 1
#else
#define SAVING_ENHANCEMENTS_DEFAULT_ENABLED 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

void SavingEnhancements_SetVRDefaults();
bool SavingEnhancements_SaveGame();
void SavingEnhancements_PersistSaveEntranceInfo();
void SavingEnhancements_ClearSaveEntranceInfo();
bool SavingEnhancements_CanSave();
void SavingEnhancements_AdvancePlaytime();

#ifdef __cplusplus
}
#endif

#endif // SAVING_ENHANCEMENTS_H
