/* 
 * Randall Schmidt
 * A task scheduler that keeps an array of tasks
 * and uses timer interrupts at regular intervals
 * to execute those tasks at approximately the time
 * specified by the caller. In this implementation
 * the caller must know an upper limit for how many
 * tasks will be scheduled at once. This kind of
 * scheme works best with a small maximum number of
 * scheduled tasks and a low time resolution.
 * Otherwise you may be using more clock cycles
 * than you'd like.
 *
 * Thanks to amandaghassaei @ http://www.instructables.com/id/Arduino-Timer-Interrupts/step2/Structuring-Timer-Interrupts/
 * for the timer setup code.
 */

#include <Arduino.h>



const int prescaler = 1024; // How many clock cycles it takes before the underlying timer (Timer1) increments
const int timerHertz = 16000000 / prescaler; // How many times per second the underlying timer (Timer1) increments. 16,000,000 is the Arduino's clock speed

int maxTasks; // The maximum number of tasks that can be scheduled at once
unsigned long tickIntervalMs; // The number of milliseconds between two instances of the scheduler looking for due tasks to execute
int compareMatchRegister; // When the underlying timer (Timer1) hits this value, it interrupts and then resets to zero

// A task consists of:
// a pointer to a function that returns void and accepts a void* argument,
// that void* argument,
// and how many ticks before the task should be executed.
struct Task
{
  void (*action)(int);
        int state;
  unsigned long delayTicks;
};

struct Task** tasks;

// _maxTasks = you cannot have more than this many tasks scheduled
// at any given time. Additional tasks will be ignored. If you have
// the maximum number of tasks scheduled, you must wait for one to
// execute. Then you can schedule another task.
// _tickIntervalMs = how often in milliseconds the scheduler
// checks to see if any tasks are due to execute. The higher this number,
// the less accurate the scheduler (your tasks won't be run exactly when
// you want them to) but the overhead is also lower. A lower number makes
// the scheduler more accurate but introduces more overhead.
// You probably only want to call this function once
void setupTaskScheduler(int _maxTasks, unsigned long _tickIntervalMs)
{
  // Get rid of any existing tasks.
  if (tasks != NULL)
  {
    for (int i = 0; i < maxTasks; i++)
    {
      Task* task = tasks[i];
      free(task); 
    }

    free(tasks);
  }

  tickIntervalMs = _tickIntervalMs;
  double interruptHertz = 1000 / _tickIntervalMs; // interruptHertz = How many times per second the caller is asking to check for and execute due tasks
  maxTasks = _maxTasks;
  tasks = (Task**)calloc(maxTasks, sizeof(Task*));

  compareMatchRegister = timerHertz / interruptHertz; // compareMatchRegister = when the underlying timer (Timer1) hits this value, it interrupts and then resets to zero
}

int scheduleTimer1Task(void (*action)(int), int state, unsigned long delayMs)
{
  // Look for an empty spot in the task array and put the new task there
  for (int i = 0; i < maxTasks; i++)
  {
    if (tasks[i] == NULL)
    {
      Task* ptr = (Task*)malloc(sizeof(struct Task));
      ptr->action = action;
      ptr->state = state;
      ptr->delayTicks = delayMs / tickIntervalMs;
      tasks[i] = ptr;
      return true; // Found an empty spot in the task list for this task
    }
  }

  return false; // No spot for this task found, scheduling request rejected
}

// Thanks to amandaghassaei @ http://www.instructables.com/id/Arduino-Timer-Interrupts/step2/Structuring-Timer-Interrupts/ for the code in this function
void startSchedulerTicking()
{
  noInterrupts();

  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = compareMatchRegister;// = (16*10^6) / (1*1024) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 1024 prescaler
  TCCR1B |= (1 << CS12) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);

  interrupts();
}

// This is the timer interrupt
ISR( TIMER1_COMPA_vect )
{
  // Look for tasks that are due to be executed
  for (int i = 0; i < maxTasks; i++)
  {
    if (tasks[i] != NULL)
    {
      Task* task = tasks[i];
      unsigned long ticks = task->delayTicks;

      if (ticks == 0) // It's due!
      {
         void (*action)(int) = task->action;
         int state = task->state;
         free(task);
         tasks[i] = NULL;
         action(state);
       }
      else
      {
        task->delayTicks--; // If the task it not due, decrement its remaining tick count
      }
    } 
  }
}
