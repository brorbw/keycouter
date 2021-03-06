#include "keycounter.h"

time_t timeSinceLastKeypress;
long keyPressesSinceLastWrite;
const time_t idleTimeToWaitBeforeFileWrite = 5; // we wait 5 seconds
char *countingFile;
int isRunning = 0;
pthread_t workThread;

void *writeToFile();
int isIdle();
void uglyPrint(char *string);

int main(int argc, const char *argv[]) {
  char *homedir = getenv("HOME");

  if (homedir == NULL) {
    printf("Unable to get countingFile from env");
    exit(1);
  }

  countingFile = strcat(homedir, "/.keyCounterData");

  timeSinceLastKeypress = 0;
  keyPressesSinceLastWrite = 0;
  // Create an event tap to retrieve keypresses
  CGEventMask eventMask =
      (CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventFlagsChanged));
  CFMachPortRef eventTap =
      CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap, 0, eventMask,
                       CGEventCallback, NULL);

  // Exit the program if unable to create the event tap.
  if (!eventTap) {
    fprintf(stderr, "ERROR: Unable to create event tap.\n");
    exit(1);
  }
  // Start workthread
  isRunning = 1;
  uglyPrint("starting thread");
  int err = pthread_create(&workThread, NULL, writeToFile, NULL);
  if (err) {
    uglyPrint("Thread error");
  }

  // Create a run loop source and add enable the event tap.
  CFRunLoopSourceRef runLoopSource =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopCommonModes);
  CGEventTapEnable(eventTap, true);

  CFRunLoopRun();

  return 0;
}

// The following callback method is invoked on every keypress.
CGEventRef CGEventCallback(CGEventTapProxy proxy, CGEventType type,
                           CGEventRef event, void *refcon) {
  if (type != kCGEventKeyDown && type != kCGEventFlagsChanged &&
      type != kCGEventKeyUp) {
    return event;
  }
  time_t tempTime = time(NULL);
  timeSinceLastKeypress = tempTime;
  ++keyPressesSinceLastWrite;

  return event;
}

void *writeToFile() {
  while (isRunning) {
    // Sleep one second and check if we are idle
    sleep(1);
    // If we are idle that there has been a keypress
    if (isIdle() && keyPressesSinceLastWrite != 0) {
      FILE *fp;
      // Check if file exists to avoid segfaults
      if (access(countingFile, F_OK) == -1) {
        fp = fopen(countingFile, "w");
        // If it does not exists write the current number
        fprintf(fp, "%lu", keyPressesSinceLastWrite);
        keyPressesSinceLastWrite = 0;
      } else {
        // If it exists write the number of presses
        fp = fopen(countingFile, "r");
        long buff[1];
        fscanf(fp, "%lu", buff);
        long newCount = buff[0] + keyPressesSinceLastWrite;
        keyPressesSinceLastWrite = 0;
        fp = fopen(countingFile, "w");
        fprintf(fp, "%lu", newCount);
      }
      // Flush and close the file
      fflush(fp);
      fclose(fp);
    }
  }
  return NULL;
}

int isIdle() {
  time_t tempTime = time(NULL);
  time_t timeDiff = tempTime - timeSinceLastKeypress;
  if (timeDiff > idleTimeToWaitBeforeFileWrite) {
    return 1;
  } else {
    return 0;
  }
}

void uglyPrint(char *string) { fprintf(stderr, "%s\n", string); }
