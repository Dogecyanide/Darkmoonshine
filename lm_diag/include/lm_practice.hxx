#ifndef LM_PRACTICE_HXX
#define LM_PRACTICE_HXX

struct PADStatus;

namespace LMPractice {

// Called immediately after the one retail PADRead. The menu keeps the physical
// port-1 sample and gives JUTGamePad a neutral sample while it owns input.
void filterPadRead(PADStatus *statuses);
void tick();
void draw(void *directPrint);
bool isOpen();

}  // namespace LMPractice

#endif  // LM_PRACTICE_HXX
