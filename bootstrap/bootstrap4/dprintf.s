#include "include/syscall.h"
#include "regs.h"

#===========================================================================
# R0: File handle
# R1: String
#===========================================================================
:_dputs
	%arg file_handle, string_ptr
	%call :_strlen, @string_ptr
	%call :syscall3 @SC_WRITE @file_handle r0
	%ret
#===========================================================================


#===========================================================================
# R0: File handle
# R1: Format string
# Stack: printf arguments
#===========================================================================
:_dprintf
	%arg file_handle, string_ptr
	%local varargs
	mov @varargs, sp
	add @varargs, @__LOCALS_SIZE__
.loop
	ld.b @tmp1, r1
	eq @tmp1, 0
	%ret?
	eq @tmp1, '%'
	jump? .percent
	# Write that char
	sys @tmp0 r1 @tmp1
	add r1, 1
	jump .loop
.percent
	add r1, 1
	ld.b @tmp1, r1
	eq @tmp1, 's'
	jump? .percent_s
	eq @tmp1, '%'
	jump? .percent_percent
	eq @tmp1, 'd'
	jump? .percent_d
	eq @tmp1, 'x'
	jump? .percent_x

	%call :fatal, .invalid_escape

.percent_percent
	%call :_dputs, @file_handle, .percent_string
	jump .loop
.percent_s
	%call :_dputs, @file_handle, @varargs
	add @varargs, 4
	jump .loop
.percent_x
.percent_d
	jump .loop

.invalid_escape
	ds "Invalid % escape"

.percent_string
	ds "%"