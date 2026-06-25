; LLVM intermediate representation
; Run with: llc file.ll && gcc file.s && ./a.out

@str = internal constant [4 x i8] c"%d\0A\00"

declare i32 @printf(i8*, ...)

define i32 @main() {
  ; Print a secret number on the screen
  %1 = select i1 true, float 0x402ABD70A0000000, float 0xC0FFEE0000000000
  %2 = fpext float %1 to double
  %3 = fmul double %2, 1.000000e+02
  %4 = fptoui double %3 to i32
  %5 = add i32 %4, 1
  ; Call printf
  call i32 (i8*, ...)* @printf(i8* getelementptr inbounds ([4 x i8]* @str, i32 0, i32 0), i32 %5)
  ret i32 0
}
