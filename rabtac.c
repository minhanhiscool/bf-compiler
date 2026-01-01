#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SIZE 2048

int main(int argc, char *argv[]) {
  bool assembly = false;
  bool object = false;
  bool optimize = false;
  int loop_cnt = 0;
  int loop_stack[MAX_SIZE];
  int stack_top = -1;

  if (argc < 2) {
    fprintf(stderr,
            "Usage: %s [options] <filename>\n"
            "Options:\n"
            "-a: output only assembly code\n"
            "-c: output only object code\n"
            "-o: optimize code\n",
            argv[0]);
    return 1;
  }

  for (int i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "-a") == 0) {
      if (object == true) {
        fprintf(stderr, "-a and -c are incompatible\n");
        return 1;
      }
      assembly = true;
    } else if (strcmp(argv[i], "-c") == 0) {
      if (assembly == true) {
        fprintf(stderr, "-a and -c are incompatible\n");
        return 1;
      }
      object = true;
    } else if (strcmp(argv[i], "-o") == 0) {
      optimize = true;
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      return 1;
    }
  }

  const char *filename = argv[argc - 1];
  FILE *fp = fopen(filename, "r");
  if (!fp) {
    perror("Could not open bf source code");
    return 1;
  }

  FILE *out = fopen("out.asm", "w");
  if (!out) {
    perror("Could not create output assembly");
    return 1;
  }

  fprintf(out, "section .text\n"
               "global _start\n"
               "_start:\n"
               "    mov rax, 9\n"
               "    mov rdi, 0\n"
               "    mov rsi, 67108864\n"
               "    mov rdx, 3\n"
               "    mov r10, 0x22\n"
               "    mov r8, -1\n"
               "    mov r9, 0\n"
               "    syscall\n"
               "    lea r12, [rax + 33554432]\n");

  typedef enum {
    IR_ADD,
    IR_DEC,
    IR_MOVR,
    IR_MOVL,
    IR_INP,
    IR_OUT,
    IR_LOOPL,
    IR_LOOPR
  } IR_Tokens;

  typedef struct {
    IR_Tokens id;
    int size;
  } IR;

  IR *ir_buf = NULL;
  size_t ir_buf_size = 0;
  size_t ir_buf_cap = 0;

  int c;
  while ((c = fgetc(fp)) != EOF) {
    if (ir_buf_size >= ir_buf_cap) {
      ir_buf_cap = ir_buf_cap == 0 ? 1 : ir_buf_cap * 2;
      IR *tmp = realloc(ir_buf, ir_buf_cap * sizeof(IR));
      if (!tmp) {
        perror("Could not allocate IR buffer");
        free(ir_buf);
        fclose(fp);
        fclose(out);
        if (remove("out.asm") != 0) {
          perror("Could not remove output assembly");
        }
        return 1;
      }
      ir_buf = tmp;
    }
    switch (c) {
    case '+':
      ir_buf[ir_buf_size].id = IR_ADD;
      break;
    case '-':
      ir_buf[ir_buf_size].id = IR_DEC;
      break;
    case '>':
      ir_buf[ir_buf_size].id = IR_MOVR;
      break;
    case '<':
      ir_buf[ir_buf_size].id = IR_MOVL;
      break;
    case '.':
      ir_buf[ir_buf_size].id = IR_OUT;
      break;
    case ',':
      ir_buf[ir_buf_size].id = IR_INP;
      break;
    case '[':
      ir_buf[ir_buf_size].id = IR_LOOPL;
      break;
    case ']':
      ir_buf[ir_buf_size].id = IR_LOOPR;
      break;
    default:
      continue;
    }
    ir_buf[ir_buf_size].size = 1;
    ir_buf_size++;
  }

  IR forward_ir_buf[ir_buf_size];
  int forward_ir_buf_idx = -1;
  if (optimize) {
    for (int i = 0; i < ir_buf_size; i++) {
      if (forward_ir_buf_idx == -1) {
        forward_ir_buf[++forward_ir_buf_idx] = ir_buf[i];
        continue;
      }
      if (ir_buf[i].id == IR_MOVR || ir_buf[i].id == IR_MOVL ||
          ir_buf[i].id == IR_ADD || ir_buf[i].id == IR_DEC) {
        if (forward_ir_buf[forward_ir_buf_idx].id == ir_buf[i].id) {
          forward_ir_buf[forward_ir_buf_idx].size++;
        } else if ((ir_buf[i].id == IR_MOVR &&
                    forward_ir_buf[forward_ir_buf_idx].id == IR_MOVL) ||
                   (ir_buf[i].id == IR_MOVL &&
                    forward_ir_buf[forward_ir_buf_idx].id == IR_MOVR) ||
                   (ir_buf[i].id == IR_ADD &&
                    forward_ir_buf[forward_ir_buf_idx].id == IR_DEC) ||
                   (ir_buf[i].id == IR_DEC &&
                    forward_ir_buf[forward_ir_buf_idx].id == IR_ADD)) {
          forward_ir_buf[forward_ir_buf_idx].size--;
          if (forward_ir_buf[forward_ir_buf_idx].size == 0) {
            forward_ir_buf_idx--;
          }

        } else {
          forward_ir_buf[++forward_ir_buf_idx] = ir_buf[i];
        }
      } else {
        forward_ir_buf[++forward_ir_buf_idx] = ir_buf[i];
      }
    }
  } else {
    memcpy(forward_ir_buf, ir_buf, ir_buf_size * sizeof(IR));
    forward_ir_buf_idx = ir_buf_size - 1;
  }

  int id;
  for (int i = 0; i <= forward_ir_buf_idx; i++) {
    switch (forward_ir_buf[i].id) {
    case IR_ADD:
      fprintf(out, "    add byte [r12], %d\n", forward_ir_buf[i].size);
      break;
    case IR_DEC:
      fprintf(out, "    sub byte [r12], %d\n", forward_ir_buf[i].size);
      break;
    case IR_MOVR:
      fprintf(out, "    add r12, %d\n", forward_ir_buf[i].size);
      break;
    case IR_MOVL:
      fprintf(out, "    sub r12, %d\n", forward_ir_buf[i].size);
      break;
    case IR_INP:
      fprintf(out, "    mov rax, 0\n"
                   "    mov rdi, 0\n"
                   "    mov rsi, r12\n"
                   "    mov rdx, 1\n"
                   "    syscall\n");
      break;
    case IR_OUT:
      fprintf(out, "    mov rax, 1\n"
                   "    mov rdi, 1\n"
                   "    mov rsi, r12\n"
                   "    mov rdx, 1\n"
                   "    syscall\n");
      break;
    case IR_LOOPL:
      if (stack_top + 1 >= MAX_SIZE) {
        fprintf(stderr, "Loops nesting too deep\n");
        free(ir_buf);
        fclose(fp);
        fclose(out);
        if (remove("out.asm") != 0) {
          perror("Could not remove output assembly");
        }
        return 1;
      }
      loop_stack[++stack_top] = loop_cnt++;
      id = loop_stack[stack_top];
      fprintf(out, "l%d:\n", id);
      fprintf(out,
              "    cmp byte [r12], 0\n"
              "    je l%d_e\n",
              id);
      break;
    case IR_LOOPR:
      if (stack_top < 0) {
        fprintf(stderr, "Unmatched loop\n");
        free(ir_buf);
        fclose(fp);
        fclose(out);
        if (remove("out.asm") != 0) {
          perror("Could not remove output assembly");
        }
        return 1;
      }
      id = loop_stack[stack_top--];
      fprintf(out, "    jmp l%d\n", id);
      fprintf(out, "l%d_e:\n", id);
      break;
    }
  }
  if (stack_top >= 0) {
    fprintf(stderr, "Unmatched loop\n");
    free(ir_buf);
    fclose(fp);
    fclose(out);
    if (remove("out.asm") != 0) {
      perror("Could not remove output assembly");
    }
    return 1;
  }
  free(ir_buf);
  fclose(fp);
  fprintf(out, "    mov rax, 60\n"
               "    mov rdi, 0\n"
               "    syscall\n");
  fclose(out);

  if (assembly) {
    return 0;
  }
  if (system("nasm -f elf64 -o out.o out.asm") != 0) {
    fprintf(stderr, "Could not compile assembly\n");
    return 1;
  }
  if (remove("out.asm") != 0) {
    perror("Could not remove out.asm");
  }

  if (object) {
    return 0;
  }

  if (system("ld -o out out.o") != 0) {
    fprintf(stderr, "Could not link object file\n");
  }
  if (remove("out.o") != 0) {
    perror("Could not remove out.o");
  }
  return 0;
}
