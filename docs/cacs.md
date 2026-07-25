# CACS — C++ Assembly Context Switch

手写 x86-64 汇编实现的协程上下文切换，用于 [hco](../) 有栈协程库。

## 栈布局

```
CACS_init 初始化后:

  [dummy]     ← rsp + 16 (不 pop，保证 ABI 对齐)
  [self]      ← rsp + 8
  [entry]     ← rsp + 0  ← rsp_ 指向这里
  ==========  stack_end (stack_ 末尾，高地址)

CACS 切走时:

  [$0]        ← rsp + 8  (dummy，匹配 pop rdi)
  [label1]    ← rsp + 0  (返回地址)  ← saved rsp 指向这里
```

关键不变量：**CACS_init 推 24 字节，pop 16 字节，净留 8 字节在栈上**；**CACS save 推 16 字节，pop 16 字节，对称**。

## 函数签名

```cpp
void CACS_init(void** rsp, void(*entry)(void*), void* self);
void CACS(void** from, void** to);
```

- `CACS_init`：在协程栈上铺设初始上下文。rsp 由调用方预置为 `stack_end`（高地址），向下推值后回写。
- `CACS`：保存当前 `from` 的 rsp，切换到 `to` 的 rsp，pop 返回地址和参数后跳转。

## 切换流程

### 首次切换到协程

```
CACS_init 铺设:  [dummy][self][entry]

CACS restore:
  pop rax → entry (task_wrapper)
  pop rdi → self (Coroutine*)
  rsp 停在 dummy 之上 (rsp mod 16 == 8)
  jmp *rax → task_wrapper(self)       ← ABI 对齐正确
```

### 协程 yield / 被调度

```
CACS save (from 侧):
  push $0       (dummy)
  push label1   (返回地址)
  保存 *from = rsp

CACS restore (to 侧):
  mov rsp, *to
  pop rax → 返回地址
  pop rdi → 0 (dummy) 或 self (首次)
  jmp *rax
```

## Callee-saved 寄存器

`CACS` 的 clobber list 声明了 `rbx, rbp, r12-r15`。由于 `CACS` 是 `always_inline` inline 到 `swap()` 中，编译器在 `swap()` 的 prologue/epilogue 自动保存/恢复这些寄存器。每个协程有独立栈，切栈后各自栈上的 saved regs 互不干扰。

**注意**：首次运行时 `CACS` 内 `jmp *rax` 直接跳到 `task_wrapper`，不会回到 `swap()` 的 epilogue。首次恢复 callee-saved regs 发生在协程第一次 yield 时——此时 regs 已经被编译器在 `swap` prologue 中保存到协程自己的栈上，取值是有效的。后续每次切回都走 `label 1` → epilogue → 正常恢复。

### CACS_init 不需要初始化 callee-saved 寄存器

因为 `jmp` 绕过了 epilogue 的 pop 路径。首次切回时从不经过 epilogue。

## 参数传递

`CACSContext::task_wrapper(void* raw_ctx)` 通过 ABI 的 `%rdi` 接收参数。`CACS_init` 推入 `self`，`CACS` 恢复时 `pop rdi` 填入。第二次及之后切回时 `pop rdi` 读到的是 dummy `$0`（无人使用，无害）。

## 踩过的坑

### 1. ABI 栈对齐：rsp mod 16 必须等于 8

**为什么 CACS save/restore 路径不受影响**：`jmp` 的落点是 `label 1`（`swap()` 函数中间），不是函数入口。`swap()` 被正常 `call` 进入时 rsp 已对齐，save/restore 对称（push 16 / pop 16）回到原位置，对齐不变量自然保持。只有 `CACS_init` 路径跳到 `task_wrapper` **函数入口**——函数边界要求 `rsp mod 16 == 8`。

**现象**：功能正确但随机 SIGSEGV（`exit code 139`），无调用栈。

**原因**：x86-64 System V ABI 要求函数入口处 `rsp mod 16 == 8`（§3.2.2: `(%rsp + 8)` 是 16 的倍数）。`call` 指令推了 8 字节返回地址后满足此条件。用 `jmp` 绕过了 `call`，入口处 `rsp mod 16 == 0`，违反 ABI。编译器在栈上生成的 `movaps` 等对齐加载指令触发 GP fault。

**修复**：`CACS_init` 多推一个 dummy（3 push = 24 字节），`CACS` 仍只 pop 2 个值（16 字节）。dummy 留在栈上不 pop，rsp 自动在 `jmp` 后满足 `rsp mod 16 == 8`。

`CACS` 的 save/restore 路径保持 push/pop 对称（都是 16 字节），不受影响——那个路径返回 `label 1`，rsp 回到进入 asm 前的位置，本来就是对齐的。

### 2. push/pop 顺序不一致导致返回地址和参数错位

**现象**：crash 或跳转到错误地址。

**原因**：`CACS_init` 推 `[self][entry]`（entry 在栈顶），`CACS` save 推 `[label1][$0]`（$0 在栈顶）。restore 统一 `pop rax; pop rdi; jmp rax`——首次从 `CACS_init` 路径 pop 到 `rax=entry, rdi=self`（正确）；后续从 save 路径 pop 到 `rax=$0, rdi=label1`（错误）。

**修复**：统一 push 顺序——栈顶永远是跳转地址。`CACS_init` 推 `[dummy][self][entry]`，`CACS` save 推 `[$0][label1]`。restore `pop rax` 始终拿到跳转目标。

### 3. clobber list 中的 `rsp` 是 UB

GCC 文档明确 x86 inline asm 中 clobber `rsp` 会产生不可预测行为。已从两处 clobber list 移除，保留 `"memory"` 足够。

### 4. 硬编码栈偏移取参数

旧代码在 `CACSContext::task_wrapper` 中用 `mov 40(%rsp), %0` 从栈上取 `self`。不同优化级别和编译器下栈帧布局不同，不可靠。

**修复**：改为通过 ABI 传参——`CACS` 中 `pop rdi` 填入 `%rdi`，`task_wrapper` 正常声明参数接收。

### 5. 双重 task_wrapper

旧代码 `CACSContext::task_wrapper` → `ContextPolicy::task_` (std::function) → `Coroutine::task_wrapper` → 用户函数，4 层间接调用。`ContextPolicy` 不应该持有 task 概念。

**修复**：`ContextPolicy::init()` 改为 `init(void(*entry)(void*), void* arg)`，`Coroutine::make_context()` 直接传 `Coroutine::task_wrapper` 和 `this`。`ContextPolicy` 回归纯粹的上下文切换原语。

### 6. yield/resume status 管理

- `yield()` 原断言 `status_ != RUNNING` 要求调用方预改状态，语义不清晰；改为自行管理：`RUNNING` 时设为 `READY`，`DEAD` 时保持不变
- `resume()` 的 `swap` 参数方向反了（`context_->swap(*running_co->context_)` 应为 `running_co->context_->swap(*context_)`）
- `task_wrapper` 必须在 `task_()` 后设 `DEAD` 再 yield，否则调度器死循环

## 已知限制

- 仅支持 x86-64 Linux（使用 System V ABI 调用约定和寄存器）
- 每次 save 向栈推 16 字节，首次 init 额外留 8 字节（dummy），1MB 栈绰绰有余
- push/pop 对称性依赖 `always_inline` 将 `CACS` 内联到 `swap()` 中——如果编译器拒绝内联，callee-saved 寄存器保护失效
