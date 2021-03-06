#include "uart.h"
#include "sched.h"
#include "fork.h"
/*
//required 1 & required 2

void foo(){
  while(1) {
    uart_puts("Task id: ");
    uart_hex(current -> id);
    uart_puts("\n");
    delay(1000000);
    Schedule();
  }
}

void idle(){
  while(1){
    Schedule();
    delay(1000000);
  }
}

void main() {
  // ...
  // boot setup
  // ...
  uart_init();
  uart_getc();
  uart_puts("MACHINE IS OPEN!!\n");

  unsigned long current_el;
  current_el = get_el();
  uart_puts("Current EL: ");
  uart_hex(current_el);
  
  int N=5;
  for(int i = 0; i < N; ++i) { // N should > 2
    privilege_task_create(PF_KTHREAD, (unsigned long)&foo, 0);
  }

  idle();
}

*/
void uart_test(){
    char buffer[16]="writewrite\n";
    buffer[12]='\0';  
    
    int size;
    size = uart_write(buffer,sizeof(buffer));
    uart_puts("size: 0x"); 
    uart_hex(size);
    uart_puts("\n"); 
   
    size = uart_read(buffer,sizeof(buffer));
    uart_puts("size: 0x"); 
    uart_hex(size);
    uart_puts("\n"); 
}

void foo(){
  int tmp = 5;
  uart_puts("Task ");
  uart_hex(get_taskid());
  uart_puts(" after exec, tmp address 0x");
  uart_hex(&tmp);
  uart_puts(", tmp value ");
  uart_hex(tmp);
  uart_puts("\n");
  
  uart_test();  

  exit(0);
  uart_puts("Should not be printed\n");
}

void test() {
  int cnt = 1;
  if (fork() == 0) {
    fork();
    delay(100000);
    fork();
    while(cnt < 10) {
      if(cnt == 6) {
        fork();
      }

      uart_puts("Task id: ");
      uart_hex(get_taskid());
      uart_puts(", cnt: ");
      uart_hex(cnt);
      uart_puts("\n");

      delay(10000);
      ++cnt;
    }
    exit(0);
    uart_puts("Should not be printed\n");
  } else {
    uart_puts("Task ");
    uart_hex(get_taskid());
    uart_puts(" before exec, cnt address 0x");
    uart_hex(&cnt);
    uart_puts(", cnt value ");
    uart_hex(cnt);
    uart_puts("\n");
  
    exec(foo);
  }
}

void zombie_reaper(){
    while(1){
        uart_puts("I'm zombie_reaper\n");
        struct task* p;
        for (int i = 0; i < NR_TASKS; i++){
            p = task_pool[i];
            if(p && p->state == TASK_ZOMBIE){
                uart_puts("free ");
                uart_hex(i);
                uart_puts("'s kernel stack and task struct.\n");
                free_page((unsigned long)p);
                task_pool[i] = 0;
            }
        } 
        Schedule();
        delay(100000);
    }
}

// -----------above is user code-------------
// -----------below is kernel code-------------

void user_test(){
  do_exec((unsigned long)&test);
}

void idle(){
  while(1){
    if(exist == 2) {
      break;
    }
    
    Schedule();
    delay(1000000);
  }
  
  Schedule();
  uart_puts("Test finished\n");
  disable_irq();
  while(1);
}

void main(void){

	uart_init();
    uart_getc();
    uart_puts("MACHINE IS OPEN!!\n");
  
    unsigned long current_el;
    current_el = get_el();
    uart_puts("Current EL: ");
    uart_hex(current_el);
    uart_puts("\n");
    
    int res;
    res = privilege_task_create(PF_KTHREAD, (unsigned long)&zombie_reaper, 0);
    if (res < 0) {
        uart_puts("error while starting kernel process");
        return;
    }
    res = privilege_task_create(PF_KTHREAD, (unsigned long)&user_test, 0);
    if (res < 0) {
        uart_puts("error while starting kernel process");
        return;
    }
    
    core_timer_enable();
    enable_irq();
    
    idle();
    
//    shell();
}

