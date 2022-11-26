good: string "GOOD"
bad: string "BAD"
flag_key: byte 0

mov [flag_key], 30
mov v0, [flag_key]
cmp v0, 30
jmp success
print bad
push 2
halt
success:
	print good
	push 1
	halt