#ifndef CACS_H_
#define CACS_H_

// 栈布局：
// CACS_init:     [dummy][self][entry], entry 在栈顶
// CACS 切走时:    [$0][label1], label1 在栈顶
// CACS 切回时:    pop rax=跳转地址, pop rdi=self或0

inline __attribute__((always_inline))
void CACS_init(void** rsp, void(*entry)(void *), void* self) {
  asm volatile (R"(
    mov %%rsp, %%rax # 保存当前栈地址
    mov (%0), %%rsp  # 切换为协程栈
    push $0          # dummy，保证 jmp 后 rsp mod 16 == 8
    push %2          # 把self存入协程栈中 8B
    push %1          # 把entry作为协程的返回地址存入协程栈中 8B
    mov %%rsp, (%0)    
    mov %%rax, %%rsp # 切换回原来的栈
  )" : 
     : "r"(rsp), "r"(entry), "r"(self)
     : "rax", "memory");
}

inline __attribute__((always_inline))
void CACS(void** from, void** to) {
  asm volatile (R"(
    lea 1f(%%rip), %%rax
    push $0              # dummy，匹配 pop rdi
    push %%rax           # 返回地址在栈顶
    mov %%rsp, (%0)      # 将当前 rsp 保存到 from
    mov (%1), %%rsp      # 将 rsp 改为目标栈 to
    pop %%rax            # 取出跳转地址
    pop %%rdi            # 取出 self(首次) 或 0(后续)
    jmp *%%rax           # 跳转
    1:
  )" : 
     : "r"(from), "r"(to) 
     : "rax", "rbx", "rcx", "rdx",
       "rbp", "r8", "r9", "r10", "r11", "r12",
       "r13", "r14", "r15");
}

#endif // CACS_H_
