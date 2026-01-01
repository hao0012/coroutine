#ifndef CACS_H_
#define CACS_H_

inline __attribute__((always_inline))
void CACS_init(void** rsp, void(*entry)(), void* self) {
  asm volatile (R"(
    mov %%rsp, %%rax # 保存当前栈地址
    mov (%0), %%rsp  # 切换为协程栈
    push %2          # 把param存入协程栈中 8B
    push %1          # 把entry作为协程的返回地址存入协程栈中 8B
    mov %%rsp, (%0)    
    mov %%rax, %%rsp # 切换回原来的栈
  )" : 
     : "r"(rsp), "r"(entry), "r"(self)
     : "rax", "rsp", "memory");
}

inline __attribute__((always_inline))
void CACS(void** from, void** to) {
  asm volatile (R"(
    lea 1f(%%rip), %%rax # rip指向当前执行的指令，lea将标签1的内容放入rax（返回地址）
    push %%rax           # 在当前栈保存返回地址1
    mov %%rsp, (%0)      # 将当前rsp保存到from
    mov (%1), %%rsp      # 将rsp改为目标栈to
    pop %%rax            # 从新栈中取出返回地址
    jmp *%%rax           # 跳转到返回地址
    1:
  )" : 
     : "r"(from), "r"(to) 
     : "rax", "rbx", "rcx", "rdx",
       "rbp", "r8", "r9", "r10", "r11", "r12",
       "r13", "r14", "r15");
}

#endif // CACS_H_