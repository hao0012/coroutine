#include <iostream>

int original(int count) {
  int sum = 0;
  for (int i = 1; i <= count; i++) {
    sum += i;
  }
  return sum;
}

int loop_unfold(int count) {
  int sum = 0;
  const int round = count / 5;
  for (int i = 1; i < 5 * round; i += 5) {
    sum += i;
    sum += i + 1;
    sum += i + 2;
    sum += i + 3;
    sum += i + 4;
  }

  switch(count % 5) {
    case 4: sum += round * 5 + 4;
    case 3: sum += round * 5 + 3;
    case 2: sum += round * 5 + 2;
    case 1: sum += round * 5 + 1;
    case 0: ;
  }

  return sum;
}

int duff_device(int count) {
  int sum = 0;
  switch(count % 5) {
            while(count > 0) {
    case 4:   sum += count--;
    case 3:   sum += count--;
    case 2:   sum += count--;
    case 1:   sum += count--;
    case 0:   sum += count--;
            }
  }
  return sum;
}

#define co_begin static int state = 0; switch(state) { case 0:
#define co_return(x) do {                   \
                          state = __LINE__; \
                          return x;         \
                          case __LINE__:;   \
                        } while(0);
#define co_finish }

int generator_co(int count) {
  static int i = 1;

  co_begin
  for (i = 1; i <= count; i++) {
    return i;
    co_return(i)
  }
  co_finish

  return 0;
}

int generator(int count) {
  static int i = 1;
  static int state = 0;
  switch(state) {
    case 0: state = 1;
            for (i = 1; i <= count; i++) {
              return i;
    case 1:;
            }
  }

  return 0;
}

// 如果是 decompressor 被重写
int decompressor(void) {
  static int repchar;
  static int replen;
  if (replen > 0) {
      replen--;
      return repchar;
  }
  char c = getchar();
  if (c == EOF) return EOF;
  if (c == 0xFF) {
      replen = getchar();
      repchar = getchar();
      replen--;
      return repchar;
  } else {
      return c;
  }
}

// 如果是 parser 被重写
void parser(int c) {
    static enum {
        START, IN_WORD
    } state;

    switch (state) {
        case IN_WORD:
        if (isalpha(c)) {
            add_to_token(c);
            return;
        }
        got_token(WORD);
        state = START;
        /* fall through */

        case START:
        add_to_token(c);
        if (isalpha(c)) {
            state = IN_WORD;
        } else {
            got_token(PUNCT);
        }
        break;
    }
}

int main() {
  int count = 12;
  int ans1 = original(count); 
  int ans2 = loop_unfold(count);
  int ans3 = duff_device(count);
  std::cout << "ans1: " << ans1 << std::endl;
  std::cout << "ans2: " << ans2 << std::endl;
  std::cout << "ans3: " << ans3 << std::endl;
}