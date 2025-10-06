; ModuleID = 'dystopia_module'
source_filename = "dystopia_module"

@.str = private unnamed_addr constant [12 x i8] c"Hello world\00", align 1

define void @main() {
entry:
  %0 = call i32 @puts(ptr @.str)
  ret void
}

declare i32 @puts(ptr)
